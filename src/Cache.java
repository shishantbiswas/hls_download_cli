import org.json.JSONObject;
import java.io.*;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;


public class Cache{
    private final Map<String, String> cache;
    private final String fileName;

    public Cache(String fileName) {
        this.fileName = fileName;
        this.cache = new HashMap<>();
        loadFromFile();
    }

    private void loadFromFile()  {
        File file = new File(fileName);
            try {
                if(!file.exists()){
                    FileOutputStream fos = new FileOutputStream("cache.json");
                    fos.write("{}".getBytes());
                    fos.close();
                }
                String content = new String(Files.readAllBytes(Paths.get(fileName)));
                JSONObject json = new JSONObject(content);
                json.keySet().forEach(key -> cache.put(key, json.getString(key)));
            } catch (IOException e) {
                System.out.println("Failed to load cache from file: " + e.getMessage());
            }
    }

    private void saveToFile() {
        JSONObject json = new JSONObject(cache);
        try (FileWriter writer = new FileWriter(fileName)) {
            writer.write(json.toString());
        } catch (IOException e) {
            System.out.println("Failed to save cache to file: " + e.getMessage());
        }
    }

    public void put(String key, String value) {
        cache.put(key, value);
        saveToFile();
    }

    // Get value by key
    public String get(String key) {
        return cache.get(key);
    }

    public void delete(String key) {
        if (cache.remove(key) != null) {
            saveToFile();
        }
    }

    public boolean containsKey(String key) {
        return cache.containsKey(key);
    }

}
