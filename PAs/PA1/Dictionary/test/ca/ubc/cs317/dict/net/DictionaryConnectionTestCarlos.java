package ca.ubc.cs317.dict.net;

import ca.ubc.cs317.dict.model.Database;
import ca.ubc.cs317.dict.model.Definition;
import ca.ubc.cs317.dict.model.MatchingStrategy;
import org.junit.jupiter.api.*;

import java.util.Collection;
import java.util.Map;
import java.util.Set;

import static org.junit.jupiter.api.Assertions.*;

/**
 * Additional tests for DictionaryConnection.
 */
public class DictionaryConnectionTestCarlos {

    @Test
    public void testBasicConnection() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        assertNotNull(conn);
        // We can explicitly close here to make sure it doesn't throw.
        conn.close();
    }

    @Test
    public void testGetDatabaseList() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        assertNotNull(dbMap, "Returned database map should not be null");
        assertTrue(dbMap.size() > 0, "Returned database map should contain at least one database");
        conn.close();
    }

    @Test
    public void testGetDefinition() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        // Commonly tested with "wn" (WordNet) if available:
        Database wn = dbMap.get("wn");
        assertNotNull(wn, "WordNet database (wn) should be available on dict.org");
        Collection<Definition> defs = conn.getDefinitions("parrot", wn);
        assertTrue(defs.size() > 0, "There should be at least one definition for 'parrot' in WordNet");
        conn.close();
    }

    /**
     * Test retrieving matching strategies.
     * Often the dict.org server supports strategies like "prefix", "exact", etc.
     * We check that we get at least one strategy back.
     */
    @Test
    public void testGetStrategyList() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Set<MatchingStrategy> strategies = conn.getStrategyList();
        assertNotNull(strategies, "Strategy set should not be null");
        assertFalse(strategies.isEmpty(), "Strategy set should not be empty");
        conn.close();
    }

    /**
     * Test retrieving database info for a known database.
     * For example, WordNet (wn) might return something containing "WordNet".
     */
    @Test
    public void testGetDatabaseInfo() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        Database wn = dbMap.get("wn");
        assertNotNull(wn, "WordNet database (wn) should be available on dict.org");
        String info = conn.getDatabaseInfo(wn);
        assertNotNull(info, "Database info string should not be null");
        assertTrue(info.length() > 0, "Database info string should not be empty");
        conn.close();
    }

    /**
     * Test retrieving a match list from the server using the "prefix" strategy.
     * We verify that we get at least one match for a known prefix (e.g. "dict").
     */
    @Test
    public void testGetMatchListPrefix() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        Database firstDb = dbMap.values().iterator().next(); // Just pick any known DB

        // We assume "dict" might show up as a prefix to many words
        MatchingStrategy prefix = new MatchingStrategy("prefix", "Matches words that begin with a given string");
        Set<String> matches = conn.getMatchList("dict", prefix, firstDb);
        assertNotNull(matches, "Match set should not be null");
        assertFalse(matches.isEmpty(), "There should be at least one match for 'dict'");
        conn.close();
    }

    /**
     * Test retrieving a match list for a nonsense word that presumably won't appear.
     * We expect the result to be empty.
     */
    @Test
    public void testGetMatchListNoResults() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        Database firstDb = dbMap.values().iterator().next();

        MatchingStrategy prefix = new MatchingStrategy("prefix", "Matches words that begin with a given string");
        Set<String> matches = conn.getMatchList("xyzabcnosuchword", prefix, firstDb);
        assertNotNull(matches, "Match set should not be null");
        assertTrue(matches.isEmpty(), "Expected zero matches for this nonsense word");
        conn.close();
    }

    /**
     * Test retrieving definitions for a nonsense word.
     * We expect the result to be empty or that the server returns a 552 response code.
     */
    @Test
    public void testGetDefinitionNoResults() throws DictConnectionException {
        DictionaryConnection conn = new DictionaryConnection("dict.org");
        Map<String, Database> dbMap = conn.getDatabaseList();
        Database firstDb = dbMap.values().iterator().next();

        Collection<Definition> defs = conn.getDefinitions("xyzabcnosuchword", firstDb);
        assertNotNull(defs, "Definitions collection should not be null");
        assertTrue(defs.isEmpty(), "Expected zero definitions for nonsense word");
        conn.close();
    }

    /**
     * Test that the connection throws an exception if the host is invalid.
     * We expect a DictConnectionException.
     */
    @Test
    public void testInvalidHost() {
        assertThrows(DictConnectionException.class, () -> {
            new DictionaryConnection("notARealHost12345.fake");
        }, "Expected DictConnectionException for an invalid host");
    }

    /**
     * Test the close() method by creating a connection and closing it.
     * We mainly want to ensure that no exceptions are thrown and that resources are cleaned.
     */
    @Test
    public void testCloseConnection() {
        DictionaryConnection conn = null;
        try {
            conn = new DictionaryConnection("dict.org");
        } catch (DictConnectionException e) {
            fail("Failed to establish connection to dict.org: " + e.getMessage());
        }
        // If we reached here, connection is successful
        assertNotNull(conn, "Connection object should not be null");
        conn.close();
    }
}
