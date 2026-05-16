package ca.ubc.cs317.dict.net;

import ca.ubc.cs317.dict.model.*;
import org.junit.jupiter.api.Test;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.*;
import java.util.concurrent.Semaphore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class DictionaryConnectionTestBailey {
    @Test
    public void testBasicConnection() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        assertNotNull(conn);
    }

    @Test
    public void testBasicDisconnect() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        assertNotNull(conn);
        conn.close();
    }

    @Test
    public void testGetDatabaseList() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();
        assertTrue(dbl.size() > 0);
    }

    @Test
    public void testGetDatabaseListContainsCommon() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();
        assertTrue(dbl.containsKey("english"));
        assertTrue(dbl.containsKey("wn"));
        assertTrue(dbl.containsKey("gcide"));
        assertTrue(dbl.containsKey("foldoc"));
        assertTrue(dbl.containsKey("jargon"));
    }

    @Test
    public void testGetDatabaseListWellFormatted() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();
        for (String key : dbl.keySet()) {
            assertEquals(key, dbl.get(key).getName());
            assertNotNull(key, dbl.get(key).getDescription());
        }
    }

    @Test
    public void testGetDatabaseInfoCanHaveNewLines() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();

        // wn is a database that you SHOULD have and the description is multiple lines
        // long
        Database wndb = dbl.get("wn");
        assertTrue(conn.getDatabaseInfo(wndb).contains("\n"));
    }

    @Test
    public void testGetDefinition() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();
        assertTrue(dbl.size() > 0);
        Database wn = dbl.get("wn");
        assertNotNull(wn);
        Collection<Definition> defs = conn.getDefinitions("parrot", wn);
        assertTrue(defs.size() > 0);
    }

    @Test
    public void testGetDefinitionWildCardDict() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");

        Database wildcard = new Database("*", "all dbs");

        Collection<Definition> defs = conn.getDefinitions("bird", wildcard);

        // wildcard will DEFINITELY contain WN
        boolean containedWN = false;
        for (Definition def : defs) {
            if (def.getDatabaseName().equals("wn")) {
                containedWN = true;
                break;
            }
        }

        assertTrue(containedWN);
    }

    @Test
    public void testGetDefinitionSingularDict() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");

        Database wildcard = new Database("!", "singular db");

        Collection<Definition> defs = conn.getDefinitions("the", wildcard);

        assertTrue(defs.size() > 0);
    }

    @Test
    public void testGetDefinitionCanHaveNewLines() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbl = conn.getDatabaseList();
        assertTrue(dbl.size() > 0);
        Database wn = dbl.get("wn");
        assertNotNull(wn);

        Collection<Definition> defs = conn.getDefinitions("than", wn);

        // definitions usually have new lines
        for (Definition def : defs) {
            assertTrue(def.getDefinition().contains("\n"));
        }
    }

    @Test
    public void testGetStrategyListCommon() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<MatchingStrategy> strats = conn.getStrategyList();

        // NOTE: matchingstrategy compares equals by name so this works
        assertTrue(strats.contains(new MatchingStrategy("prefix", null)));
        assertTrue(strats.contains(new MatchingStrategy("exact", null)));
        assertTrue(strats.contains(new MatchingStrategy("first", null)));
        assertTrue(strats.contains(new MatchingStrategy("last", null)));
    }

    @Test
    public void testGetStrategyListWellFormed() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<MatchingStrategy> strats = conn.getStrategyList();

        for (MatchingStrategy matchingStrategy : strats) {
            assertNotNull(matchingStrategy.getName());
            assertNotNull(matchingStrategy.getDescription());
        }
    }

    @Test
    public void testGetMatchListSimple() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<String> matches = conn.getMatchList("bird", new MatchingStrategy("exact", null), new Database("wn", null));
        assertTrue(matches.size() > 0);
    }

    @Test
    public void testGetMatchListWildcard() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<String> matches = conn.getMatchList("bird", new MatchingStrategy("exact", null), new Database("*", null));
        assertTrue(matches.size() > 0);
    }

    @Test
    public void testGetMatchListSingle() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<String> matches = conn.getMatchList("bird", new MatchingStrategy("exact", null), new Database("!", null));
        assertTrue(matches.size() > 0);
    }

    @Test
    public void testGetMatchListWildcardVariousStrats() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<String> matches1 = conn.getMatchList("bird", new MatchingStrategy("exact", null), new Database("*", null));
        assertTrue(matches1.size() > 0);
        Set<String> matches2 = conn.getMatchList("a", new MatchingStrategy("prefix", null), new Database("*", null));
        assertTrue(matches2.size() > 0);
        Set<String> matches3 = conn.getMatchList("e", new MatchingStrategy("last", null), new Database("*", null));
        assertTrue(matches3.size() > 0);
        Set<String> matches4 = conn.getMatchList("z", new MatchingStrategy("first", null), new Database("*", null));
        assertTrue(matches4.size() > 0);
    }

    private static class BadServer {
        public static final int PORT = 2826;
        private static Thread instance;
        private ServerSocket listener;

        public static void startBadServer() throws Exception {
            if (instance != null) {
                return;
            }

            Semaphore serverStarted = new Semaphore(0);

            instance = new Thread(new Runnable() {
                public void run() {
                    new BadServer(serverStarted);
                }
            });
            instance.start();

            serverStarted.acquire();
        }

        private BadServer(Semaphore onListening) {
            try {
                doServer(onListening);
            } catch (Exception e) {
                // too bad!
            }
        }

        private void doServer(Semaphore onListening) throws Exception {

            listener = new ServerSocket(PORT);
            onListening.release();

            // listen to clients
            while (true) {
                Socket client = listener.accept();
                Thread clientHandler = new Thread(new Runnable() {
                    public void run() {
                        try {
                            BufferedReader fromClient = new BufferedReader(
                                    new InputStreamReader(client.getInputStream()));
                            PrintWriter toClient = new PrintWriter(client.getOutputStream(), true);
                            toClient.println("220 WELCOME TO BAD SERVER >:)");
                            while (fromClient.readLine() != null) {
                                toClient.println("500 BadServer always fails!");
                            }
                        } catch (Exception e) {
                            // too bad!
                        }
                    }
                });
                clientHandler.start();
            }

        }
    }

    @Test
    public void testFailConnections() {
        try {
            DictionaryConnection conn = new DictionaryConnection("nobodyhost", 555555);
            fail();
        } catch (DictConnectionException dce) {
            // good!
        }
    }

    @Test
    public void testGetDataBaseListBadServer() throws Exception {
        BadServer.startBadServer();
        DictionaryConnection conn = new DictionaryConnection("localhost", BadServer.PORT);
        Map<String, Database> dbset = conn.getDatabaseList();
        assertEquals(dbset.size(), 0);
    }

    @Test
    public void testGetStrategyListBadServer() throws Exception {
        BadServer.startBadServer();
        DictionaryConnection conn = new DictionaryConnection("localhost", BadServer.PORT);
        Set<MatchingStrategy> dbset = conn.getStrategyList();
        assertEquals(dbset.size(), 0);
    }

    @Test
    public void testGetDefinitionsListBadServer() throws Exception {
        BadServer.startBadServer();
        DictionaryConnection conn = new DictionaryConnection("localhost", BadServer.PORT);
        Collection<Definition> dbset = conn.getDefinitions("bird", new Database("*", null));
        assertEquals(dbset.size(), 0);
    }

    @Test
    public void testGetMatchListBadServer() throws Exception {
        BadServer.startBadServer();
        DictionaryConnection conn = new DictionaryConnection("localhost", BadServer.PORT);
        Set<String> dbset = conn.getMatchList("bird", new MatchingStrategy("prefix", null), new Database("*", null));
        assertEquals(dbset.size(), 0);
    }
}
