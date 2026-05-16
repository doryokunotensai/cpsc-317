# DICT Client (PA1)

## Requirements
- Java 11
- Gradle (install via `brew install gradle`)

## Setup
```bash
cd PAs/PA1/Dictionary
gradle wrapper
```

## Run GUI
```bash
# from PAs/PA1/Dictionary/
javac -d out -sourcepath src src/ca/ubc/cs317/dict/ui/DictionaryMain.java
java -cp out ca.ubc.cs317.dict.ui.DictionaryMain
```
Or open the project in IntelliJ and run `DictionaryMain.java`.

## Run Tests
```bash
# all tests
./gradlew test

# single test class
./gradlew test --tests "ca.ubc.cs317.dict.net.DictionaryConnectionTest"
```

Test report: `build/reports/tests/test/index.html`
