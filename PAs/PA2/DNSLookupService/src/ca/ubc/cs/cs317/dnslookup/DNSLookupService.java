package ca.ubc.cs.cs317.dnslookup;

import java.io.*;
import java.net.*;
import java.util.*;

import static ca.ubc.cs.cs317.dnslookup.DNSMessage.MAX_DNS_MESSAGE_LENGTH;

public class DNSLookupService {

	public static final int DEFAULT_DNS_PORT = 53;
	private static final int MAX_INDIRECTION_LEVEL_NS = 10;
	private static final int MAX_QUERY_ATTEMPTS = 3;
	private static final int SO_TIMEOUT = 5000;

	private final DNSCache cache = DNSCache.getInstance();
	private final Random random = new Random();
	private final DNSVerbosePrinter verbose;
	private final DatagramSocket socket;

	public DNSLookupService(DNSVerbosePrinter verbose) throws SocketException, UnknownHostException {
		this.verbose = verbose;
		socket = new DatagramSocket();
		socket.setSoTimeout(SO_TIMEOUT);
	}

	public void close() {
		socket.close();
	}

	public Collection<ResourceRecord> iterativeQuery(DNSQuestion question) throws DNSErrorException {
		Set<InetAddress> tried = new HashSet<>();

		for (int i = 0; i < MAX_INDIRECTION_LEVEL_NS; i++) {
			List<ResourceRecord> cached = cache.getCachedResults(question);
			if (containsAnswer(cached, question)) return cached;
			if (cached.stream().anyMatch(r -> r.getRecordType() == RecordType.CNAME)) return cached;

			List<ResourceRecord> nameservers = cache.getBestNameservers(question);
			List<ResourceRecord> withIPs = cache.filterByKnownIPAddress(nameservers);

			InetAddress serverAddr = null;

			if (!withIPs.isEmpty()) {
				for (ResourceRecord r : withIPs) {
					if (!tried.contains(r.getInetResult())) {
						serverAddr = r.getInetResult();
						break;
					}
				}
			} else {
				for (ResourceRecord ns : nameservers) {
					Collection<ResourceRecord> resolved = iterativeQuery(
						new DNSQuestion(ns.getTextResult(), RecordType.A, RecordClass.IN));
					for (ResourceRecord r : resolved) {
						if (r.getRecordType() == RecordType.A && !tried.contains(r.getInetResult())) {
							serverAddr = r.getInetResult();
							break;
						}
					}
					if (serverAddr != null) break;
				}
			}

			if (serverAddr == null) break;
			tried.add(serverAddr);
			individualQueryProcess(question, serverAddr);
		}

		return cache.getCachedResults(question);
	}

	private boolean containsAnswer(Collection<ResourceRecord> rrs, DNSQuestion question) {
		for (ResourceRecord rr : rrs) {
			if (rr.getQuestion().equals(question) && rr.getRecordType() == question.getRecordType()) {
				return true;
			}
		}
		return false;
	}

	public Collection<ResourceRecord> getResultsFollowingCNames(DNSQuestion question, int maxIndirectionLevels)
			throws DNSErrorException {

		if (maxIndirectionLevels < 0) throw new DNSErrorException("CNAME indirection limit exceeded");

		Collection<ResourceRecord> directResults = iterativeQuery(question);
		if (containsAnswer(directResults, question)) {
			return directResults;
		}

		Set<ResourceRecord> newResults = new HashSet<>();
		for (ResourceRecord record : directResults) {
			newResults.add(record);
			if (record.getRecordType() == RecordType.CNAME) {
				newResults.addAll(getResultsFollowingCNames(
						new DNSQuestion(record.getTextResult(), question.getRecordType(), question.getRecordClass()),
						maxIndirectionLevels - 1));
			}
		}
		return newResults;
	}

	public Set<ResourceRecord> individualQueryProcess(DNSQuestion question, InetAddress server)
			throws DNSErrorException {
		DNSMessage query = buildQuery(question);
		byte[] queryBytes = query.getUsed();
		int transactionID = query.getID();

		for (int attempt = 0; attempt < MAX_QUERY_ATTEMPTS; attempt++) {
			try {
				verbose.printQueryToSend("UDP", question, server, transactionID);
				DatagramPacket sendPacket = new DatagramPacket(queryBytes, queryBytes.length, server, DEFAULT_DNS_PORT);
				socket.send(sendPacket);

				byte[] recvBuf = new byte[MAX_DNS_MESSAGE_LENGTH];
				DatagramPacket recvPacket = new DatagramPacket(recvBuf, recvBuf.length);

				while (true) {
					socket.receive(recvPacket);
					DNSMessage response = new DNSMessage(recvBuf, recvPacket.getLength());
					if (response.getID() != transactionID || !response.getQR()) continue;

					try {
						return processResponse(response);
					} catch (DNSReplyTruncatedException e) {
						return sendViaTCP(query, question, server, transactionID);
					}
				}
			} catch (SocketTimeoutException e) {
				// retry
			} catch (IOException e) {
				return null;
			}
		}
		return null;
	}

	private Set<ResourceRecord> sendViaTCP(DNSMessage query, DNSQuestion question, InetAddress server, int transactionID)
			throws DNSErrorException {
		verbose.printQueryToSend("TCP", question, server, transactionID);
		byte[] queryBytes = query.getUsed();
		try (Socket tcpSocket = new Socket(server, DEFAULT_DNS_PORT)) {
			DataOutputStream out = new DataOutputStream(tcpSocket.getOutputStream());
			out.writeShort(queryBytes.length);
			out.write(queryBytes);
			out.flush();

			DataInputStream in = new DataInputStream(tcpSocket.getInputStream());
			int length = in.readUnsignedShort();
			byte[] recvBuf = new byte[length];
			in.readFully(recvBuf);

			DNSMessage response = new DNSMessage(recvBuf, length);
			try {
				return processResponse(response);
			} catch (DNSReplyTruncatedException e) {
				return null;
			}
		} catch (IOException e) {
			return null;
		}
	}

	public DNSMessage buildQuery(DNSQuestion question) {
		short id = (short) random.nextInt(0x10000);
		DNSMessage message = new DNSMessage(id);
		message.addQuestion(question);
		return message;
	}

	public Set<ResourceRecord> processResponse(DNSMessage message) throws DNSErrorException, DNSReplyTruncatedException {
		int rcode = message.getRcode();
		if (rcode != 0) throw new DNSErrorException("RCode is " + rcode);
		if (message.getTC()) throw new DNSReplyTruncatedException("TC bit set");

		verbose.printResponseHeaderInfo(message.getID(), message.getAA(), message.getTC(), rcode);

		int qdCount = message.getQDCount();
		int anCount = message.getANCount();
		int nsCount = message.getNSCount();
		int arCount = message.getARCount();

		for (int i = 0; i < qdCount; i++) message.getQuestion();

		Set<ResourceRecord> result = new HashSet<>();

		verbose.printAnswersHeader(anCount);
		for (int i = 0; i < anCount; i++) {
			ResourceRecord rr = message.getRR();
			cache.addResult(rr);
			verbose.printIndividualResourceRecord(rr, rr.getRecordType().getCode(), rr.getRecordClass().getCode());
			result.add(rr);
		}

		verbose.printNameserversHeader(nsCount);
		for (int i = 0; i < nsCount; i++) {
			ResourceRecord rr = message.getRR();
			cache.addResult(rr);
			verbose.printIndividualResourceRecord(rr, rr.getRecordType().getCode(), rr.getRecordClass().getCode());
			result.add(rr);
		}

		verbose.printAdditionalInfoHeader(arCount);
		for (int i = 0; i < arCount; i++) {
			ResourceRecord rr = message.getRR();
			cache.addResult(rr);
			verbose.printIndividualResourceRecord(rr, rr.getRecordType().getCode(), rr.getRecordClass().getCode());
			result.add(rr);
		}

		return result;
	}

	public static class DNSErrorException extends Exception {
		public DNSErrorException(String msg) {
			super(msg);
		}
	}

	public static class DNSReplyTruncatedException extends Exception {
		public DNSReplyTruncatedException(String msg) {
			super(msg);
		}
	}
}
