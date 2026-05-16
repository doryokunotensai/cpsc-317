package ca.ubc.cs317.dict.net;

import ca.ubc.cs317.dict.model.Database;
import ca.ubc.cs317.dict.model.Definition;
import ca.ubc.cs317.dict.model.MatchingStrategy;

import java.io.BufferedReader;
import java.io.PrintWriter;
import java.net.Socket;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.*;

/**
 * Created by Jonatan on 2017-09-09.
 */
public class DictionaryConnection {
	private static final int DEFAULT_PORT = 2628;
	private Socket socket;
	private BufferedReader in;
	private PrintWriter out;

    /** Establishes a new connection with a DICT server using an explicit host and port number, and handles initial
     * welcome messages.
     *
     * @param host Name of the host where the DICT server is running
     * @param port Port number used by the DICT server
     * @throws DictConnectionException If the host does not exist, the connection can't be established, or the messages
     * don't match their expected value.
     */
    public DictionaryConnection(String host, int port) throws DictConnectionException {
    	    // makes the connection
    	    try {
    	    	    this.socket = new Socket(host, port);
    	    	    this.out = new PrintWriter(this.socket.getOutputStream(), true);
    	    	    this.in = new BufferedReader(new InputStreamReader(this.socket.getInputStream()));
    	    } catch (IOException | IllegalArgumentException e) {
    	    	    throw new DictConnectionException(e);
    	    }

    	    Status status = Status.readStatus(in);
    	    if (status.getStatusCode() != 220) {
    	    	    throw new DictConnectionException("Expected 220, got " + status.getStatusCode());
    	    }
    }

    /** Establishes a new connection with a DICT server using an explicit host, with the default DICT port number, and
     * handles initial welcome messages.
     *
     * @param host Name of the host where the DICT server is running
     * @throws DictConnectionException If the host does not exist, the connection can't be established, or the messages
     * don't match their expected value.
     */
    public DictionaryConnection(String host) throws DictConnectionException {
        this(host, DEFAULT_PORT);
    }

    /** Sends the final QUIT message and closes the connection with the server. This function ignores any exception that
     * may happen while sending the message, receiving its reply, or closing the connection.
     *
     */
    public synchronized void close() {
    	    try {
    	    	    this.out.println("QUIT");
    	    	    Status.readStatus(this.in);
    	    	    this.socket.close();
    	    } catch (Exception e) {
    	    	    // ignore
    	    }
    }

    /** Requests and retrieves all definitions for a specific word.
     *
     * @param word The word whose definition is to be retrieved.
     * @param database The database to be used to retrieve the definition. A special database may be specified,
     *                 indicating either that all regular databases should be used (database name '*'), or that only
     *                 definitions in the first database that has a definition for the word should be used
     *                 (database '!').
     * @return A collection of Definition objects containing all definitions returned by the server.
     * @throws DictConnectionException If the connection was interrupted or the messages don't match their expected value.
     */
    public synchronized Collection<Definition> getDefinitions(String word, Database database) throws DictConnectionException {
        Collection<Definition> set = new ArrayList<>();

        // TODO Add your code here

        return set;
    }

    /** Requests and retrieves a list of matches for a specific word pattern.
     *
     * @param word     The word whose definition is to be retrieved.
     * @param strategy The strategy to be used to retrieve the list of matches (e.g., prefix, exact).
     * @param database The database to be used to retrieve the definition. A special database may be specified,
     *                 indicating either that all regular databases should be used (database name '*'), or that only
     *                 matches in the first database that has a match for the word should be used (database '!').
     * @return A set of word matches returned by the server.
     * @throws DictConnectionException If the connection was interrupted or the messages don't match their expected value.
     */
    public synchronized Set<String> getMatchList(String word, MatchingStrategy strategy, Database database) throws DictConnectionException {
        Set<String> set = new LinkedHashSet<>();

        // TODO Add your code here

        return set;
    }

    /** Requests and retrieves a map of database name to an equivalent database object for all valid databases used in the server.
     *
     * @return A map of Database objects supported by the server.
     * @throws DictConnectionException If the connection was interrupted or the messages don't match their expected value.
     */
    public synchronized Map<String, Database> getDatabaseList() throws DictConnectionException {
        Map<String, Database> databaseMap = new HashMap<>();
        try {
        	out.println("SHOW DATABASES");
        	Status status = Status.readStatus(in);
        	if (status.isNegativeReply()) return databaseMap;
        	String line;
		while (!(line = in.readLine()).equals(".")) {
	  		String[] atoms = DictStringParser.splitAtoms(line);
	  		databaseMap.put(atoms[0], new Database(atoms[0], atoms[1]));
		}
		Status.readStatus(in);

	} catch (IOException e) {
		throw new DictConnectionException(e);
	}
        return databaseMap;
    }

    /** Requests and retrieves a list of all valid matching strategies supported by the server.
     *
     * @return A set of MatchingStrategy objects supported by the server.
     * @throws DictConnectionException If the connection was interrupted or the messages don't match their expected value.
     */
    public synchronized Set<MatchingStrategy> getStrategyList() throws DictConnectionException {
        Set<MatchingStrategy> set = new LinkedHashSet<>();

        // TODO Add your code here

        return set;
    }

    /** Requests and retrieves detailed information about the currently selected database.
     *
     * @return A string containing the information returned by the server in response to a "SHOW INFO <db>" command.
     * @throws DictConnectionException If the connection was interrupted or the messages don't match their expected value.
     */
    public synchronized String getDatabaseInfo(Database d) throws DictConnectionException {
    	    StringBuilder sb = new StringBuilder();
    	    out.println("SHOW INFO " + d.getName());
    	    Status status = Status.readStatus(in);
    	    if (status.isNegativeReply()) return sb.toString();

    	    try {
    	    	    String line;
    	    	    while (!(line = in.readLine()).equals(".")) {
    	    	    	    if (sb.length() > 0) sb.append("\n");
    	    	    	    sb.append(line);
    	    	    }
    	    } catch (IOException e) {
    	    	    throw new DictConnectionException(e);
    	    }

    	    Status.readStatus(in);
    	    return sb.toString();
    }
}
