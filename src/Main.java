import java.io.*;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.security.SecureRandom;
import java.time.Duration;
import java.util.Arrays;
import java.util.Objects;

public class Main {

    public static void main(String[] args) {

        if (args.length == 0) {
            System.err.println("Argument Error: url not provided ");
            return;
        }

        final Cache kv = new Cache("cache.json");

        String url = args[0];
        String folder_name;

        if (kv.containsKey(url)) {
            folder_name = kv.get(url);
        } else if(args.length == 2) {
            folder_name = args[1];
            kv.put(url, folder_name);
        } else {
            folder_name = generateRandomString(16);
            kv.put(url, folder_name);
        }

        try {
            String res = getRequest(url);
            String[] responseArr = res.split("\n");
            int segmentArrLength = responseArr.length;
            int index = 0;
            boolean isPending = true;

            do {
                String line = responseArr[index].trim();
                if (!line.startsWith("#") && line.length() > 1) {

                    File file = new File(folder_name + "/" + extractSegment(line));
                    String filename = extractSegment(line);

                    if (file.exists() && file.length() != 0) {
                        System.out.print("\rcontinuing download after " + filename);
                        index++;
                        isPending = index <= (segmentArrLength - 1);
                    } else {
                        String segmentUrl = makeUrl(url, line);
                        int percent = (int) (((double) index / segmentArrLength) * 100);
                        System.out.print("\rDownloading " + percent + "% " + " %s / %s".formatted(index, segmentArrLength));

                        boolean downloadSuccess = downloadSegment(folder_name, segmentUrl, filename);
                        if (downloadSuccess) {
                            mergeFile(filename, folder_name);
                            index++;
                            isPending = index <= (segmentArrLength - 1);
                        }
                    }
                } else {
                    index++;
                    isPending = index <= (segmentArrLength - 1);
                }
            } while (isPending);

            System.out.println("\rDownload Completed, Starting Merge");

            String userHome = System.getProperty("user.home");
            Path downloadPath = Paths.get(userHome, "Downloads");
            String outputFolder = downloadPath + "/" + folder_name + ".mp4";
            int exitCode = ffmpeg(folder_name, outputFolder);

            if (exitCode == 0) {
                System.out.println("\nMerge was Successful");
                kv.delete(url);
                File file = new File(folder_name);
                deleteDirectory(file);
            } else {
                System.out.println("FFmpeg exited with code: " + exitCode);
            }
            System.out.println("Video saved at " + outputFolder);

        } catch (Exception e) {
            System.err.println("\nRequest failed: " + e.getMessage());
        }
    }

    private static boolean downloadSegment(String folder_name, String url, String filename) {
        try {
            Path path = Paths.get(folder_name);
            if (!Files.exists(path) && !Files.isDirectory(path)) {
                Files.createDirectories(path);
            }
            FileOutputStream fos = new FileOutputStream(folder_name + "/" + filename, false);
            byte[] data = getRequestBytes(url.trim());
            fos.write(data);
            fos.close();
            return true;
        } catch (Exception e) {
            System.out.println(filename + " failed");
            return false;
        }
    }

    public static String generateRandomString(int length) {
        final String CHARACTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        final SecureRandom random = new SecureRandom();
        StringBuilder sb = new StringBuilder(length);
        for (int i = 0; i < length; i++) {
            int randomIndex = random.nextInt(CHARACTERS.length());
            sb.append(CHARACTERS.charAt(randomIndex));
        }
        return sb.toString();
    }

    private static String getRequest(String url) throws Exception {
        HttpClient client = HttpClient.newHttpClient();
        HttpRequest request = HttpRequest.newBuilder().uri(URI.create(url)).GET().build();

        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());

        if (response.statusCode() != 200) {
            throw new RuntimeException("Failed to fetch data, HTTP status code: " + response.statusCode());
        }
        return response.body();
    }

    public static byte[] getRequestBytes(String url) throws Exception {
        HttpClient client = HttpClient.newHttpClient();

        HttpRequest request = HttpRequest.newBuilder().timeout(Duration.ofSeconds(10)).uri(URI.create(url)).GET().build();


        HttpResponse<byte[]> response;
        response = client.send(request, HttpResponse.BodyHandlers.ofByteArray());

        if (response.statusCode() != 200) {
            throw new RuntimeException("Failed to fetch data, HTTP status code: " + response.statusCode());
        }

        return response.body();
    }

    private static String makeUrl(String url, String segment) {
        if (segment.startsWith("http")) {
            return segment;
        } else {
            String[] parts = url.split("/");
            String merged = String.join("/", Arrays.copyOfRange(parts, 0, parts.length - 1));
            return merged + "/" + segment;
        }
    }

    private static String extractSegment(String segment) {
        String[] parts = segment.split("/");
        return parts[parts.length - 1].trim();
    }

    private static void mergeFile(String file, String folder_name) throws IOException {
        Path path = Paths.get(folder_name);
        if (!Files.exists(path) && !Files.isDirectory(path)) {
            Files.createDirectories(path);
        }
        try (FileOutputStream fos = new FileOutputStream(folder_name + "/merge_file.txt", true)) {
            String content = """
                    file '%s'
                    """.formatted(file.trim());
            fos.write(content.getBytes(StandardCharsets.UTF_8));
        }

    }

    private static int ffmpeg(String folder_name, String outputFilePath) throws IOException, InterruptedException {
        File file = new File("./ffmpeg");

        if (!file.exists()){
            System.out.println("ffmpeg command missing");
            return 1;
        }
            ProcessBuilder processBuilder = new ProcessBuilder("./ffmpeg", "-f", "concat", "-safe", "0", "-i", folder_name + "/merge_file.txt", "-c", "copy", outputFilePath);

        processBuilder.directory(new File("."));
        processBuilder.redirectErrorStream(true);

        Process process = processBuilder.start();

        try (BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()))) {
            String line;
            while ((line = reader.readLine()) != null) {
                System.out.printf("\n %s%n", line);
            }
        }

        return process.waitFor();
    }

    public static void deleteDirectory(File file) {
        for (File subfile : Objects.requireNonNull(file.listFiles())) {

            if (subfile.isDirectory()) {
                deleteDirectory(subfile);
            }

            subfile.delete();
        }
        file.delete();
    }
}


class MyThread extends Thread {
    private final String url;
    private final String filename;
    private final String folder_name;

    public MyThread(String url, String filename, String folder_name) {
        this.url = url;
        this.filename = filename;
        this.folder_name = folder_name;
    }

    static byte[] getRequestBytes(String url) throws Exception {
        HttpClient client = HttpClient.newHttpClient();
        HttpRequest request = HttpRequest.newBuilder().uri(URI.create(url)).GET().build();

        HttpResponse<byte[]> response = client.send(request, HttpResponse.BodyHandlers.ofByteArray());
        if (response.statusCode() != 200) {
            throw new RuntimeException("Failed to fetch data, HTTP status code: " + response.statusCode());
        }
        return response.body();
    }

    @Override
    public void run() {
        try {
            Path path = Paths.get(folder_name);
            if (!Files.exists(path) && !Files.isDirectory(path)) {
                Files.createDirectories(path);
            }
            FileOutputStream fos = new FileOutputStream(folder_name + "/" + filename);
            byte[] data = getRequestBytes(url);
            fos.write(data);
            fos.close();

        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }
}

