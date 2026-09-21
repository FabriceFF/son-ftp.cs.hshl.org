/*
Exercise 02 - FTP/SFTP client, STARTING POINT (without interface)

This program can already do everything the exercise asks for: it lists the
directories on a server, creates a new one, lists again, deletes it and lists
one final time - once via FTP and once via SFTP.

It was, however, written the way such things often grow in reality: two
developers implemented one protocol each, independently of one another.

    FTP   ->  FTP_Connection    (uses libcurl)
    SFTP  ->  SFTP_Connection   (uses libssh)

Both classes work. But they have NOTHING in common: no shared base class, and
they do not even agree on how their methods are named or what they return:

    FTP_Connection                        SFTP_Connection
    --------------------------------      --------------------------------
    bool open()                           int  connect()      // 0 == OK
    bool close()                          int  disconnect()   // 0 == OK
    std::vector<std::string>              std::vector<SFTP_Directory>
        listDirectories(path)                 getDirectories(dir)
    bool makeDir(parentPath, name)        bool createDirectory(parent, name)
    bool removeDir(fullPath)              bool deleteDirectory(dir)

Neither naming is "wrong" - each one follows the library underneath it.
libcurl works with URL strings and returns a text listing, so plain strings
are the obvious choice. libssh hands out structured attributes, so an object
is the obvious choice there.

The consequence is visible at the bottom of this file: runFtpDemo() and
runSftpDemo() do exactly the same thing, but not a single line can be shared
between them. main() has to call both of them by name - the two connections
have no common type, so they cannot even be put into one list and looped over.

YOUR TASK: see with_interface.cpp.
*/

#include <curl/curl.h>      // FTP
#include <libssh/libssh.h>  // SFTP: the SSH connection underneath
#include <libssh/sftp.h>    // SFTP: the file transfer channel on top of it

#include <iostream>
#include <sstream>  // std::istringstream, used to parse the FTP listing
#include <string>
#include <vector>

// Everything inside this unnamed namespace is visible only in this file -
// roughly what "private" is for a class, but for free functions.
namespace {

// Appends name to directory, ensuring exactly one slash in between.
std::string joinPath(const std::string& directory, const std::string& name) {
    if (!directory.empty() && directory.back() == '/') {
        return directory + name;
    }
    return directory + "/" + name;
}

// Last path component, e.g. "/upload/gruppe3" -> "gruppe3".
std::string baseName(const std::string& path) {
    size_t lastSlash = path.find_last_of('/');
    if (lastSlash == std::string::npos) {
        return path;
    }
    return path.substr(lastSlash + 1);
}

// Seconds after which a connection attempt is aborted (e.g. without VPN).
constexpr long connectTimeoutSeconds = 10;

}  // namespace

// ---------------------------------------------------------------------------
//  FTP via libcurl
//
//  libcurl is a C library, so there are no objects: you get a "handle"
//  (CURL*) which is passed into every function. The pattern is always
//
//      curl_easy_setopt(handle, OPTION, value);   // set up the request
//      curl_easy_perform(handle);                 // actually run it
//
//  Note how this class talks about PATHS (plain std::string) everywhere -
//  that is simply what libcurl gives you.
// ---------------------------------------------------------------------------
class FTP_Connection {
    std::string host;
    int port;
    std::string userpwd;
    CURL* curl = nullptr;

    // libcurl does not hand the server's answer back as a return value.
    // Instead it calls this function repeatedly with chunks of the data as
    // they arrive, and we append them to a std::string.
    // The function has to be static, because libcurl expects a plain C
    // function pointer and those cannot carry a "this".
    static size_t writeCallback(void* contents, size_t size, size_t count, void* userp) {
        auto* out = static_cast<std::string*>(userp);
        out->append(static_cast<char*>(contents), size * count);
        return size * count;
    }

    // Builds the complete URL, e.g. "ftp://son-ftp.cs.hshl.org:21/upload/".
    std::string url(const std::string& path) const {
        return "ftp://" + host + ":" + std::to_string(port) + path;
    }

    // Sets the options that are the same for every call.
    void applyCommonOptions(const std::string& requestUrl) {
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, requestUrl.c_str());
        curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd.c_str());
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, connectTimeoutSeconds);
    }

    // Executes an FTP command (MKD, RMD, ...) directly on the server.
    bool runQuoteCommand(const std::string& workingDirectory, const std::string& command,
                         const std::string& errorContext) {
        struct curl_slist* quote = curl_slist_append(nullptr, command.c_str());
        if (!quote) {
            std::cerr << errorContext << ": could not create command list\n";
            return false;
        }
        applyCommonOptions(url(workingDirectory));
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        curl_easy_setopt(curl, CURLOPT_QUOTE, quote);
        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(quote);
        if (res != CURLE_OK) {
            std::cerr << errorContext << ": " << curl_easy_strerror(res) << "\n";
            return false;
        }
        return true;
    }

    // Parses a listing in "ls -l" format. Lines starting with 'd' are
    // directories; the name starts at the 9th field (and may contain spaces).
    static std::vector<std::string> parseDirectoryNames(const std::string& listing) {
        std::vector<std::string> names;
        std::istringstream stream(listing);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty() || line[0] != 'd') {
                continue;
            }
            std::istringstream fields(line);
            std::string field;
            // Skip the first 8 fields (permissions, number of links, owner,
            // group, size, month, day, time). Everything after them is the
            // name - read with getline, because it may contain spaces.
            for (int i = 0; i < 8 && (fields >> field); ++i) {
            }
            std::string name;
            std::getline(fields, name);
            size_t nameStart = name.find_first_not_of(" \t");
            if (nameStart == std::string::npos) {
                continue;
            }
            name = name.substr(nameStart);
            if (name != "." && name != "..") {
                names.push_back(name);
            }
        }
        return names;
    }

public:
    FTP_Connection(const std::string& host, int port, const std::string& username, const std::string& password)
        : host(host), port(port), userpwd(username + ":" + password) {}

    // The class owns a CURL handle. Copying would cause it to be freed twice
    // ("Rule of Five"), so copying and moving are forbidden.
    FTP_Connection(const FTP_Connection&) = delete;
    FTP_Connection& operator=(const FTP_Connection&) = delete;
    FTP_Connection(FTP_Connection&&) = delete;
    FTP_Connection& operator=(FTP_Connection&&) = delete;

    ~FTP_Connection() {
        close();
    }

    // NOTE the name: "open" / "close" - the SFTP class calls this
    // "connect" / "disconnect".
    //
    // FTP has no real "log in and stay connected" step in libcurl's API.
    // We create the handle and send one request that transfers no data, just
    // to check that host, port, user name and password actually work.
    bool open() {
        close();
        curl = curl_easy_init();
        if (!curl) {
            std::cerr << "Could not create CURL handle\n";
            return false;
        }
        applyCommonOptions(url("/"));
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Connection failed: " << curl_easy_strerror(res) << "\n";
            close();
            return false;
        }
        return true;
    }

    // curl_easy_reset() would only reset the options but keep the connection
    // open. Only curl_easy_cleanup() actually closes it.
    bool close() {
        if (curl) {
            curl_easy_cleanup(curl);
            curl = nullptr;
        }
        return true;
    }

    // NOTE the return type: only the NAMES of the directories, as strings.
    // The caller has to build the full paths itself. The SFTP class returns
    // ready-made objects that already know their path.
    std::vector<std::string> listDirectories(const std::string& path) {
        std::vector<std::string> names;
        if (!curl) {
            std::cerr << "No open connection\n";
            return names;
        }
        std::string requestPath = path;
        if (requestPath.empty() || requestPath.back() != '/') {
            requestPath += "/";
        }
        // A plain request on a DIRECTORY url makes the server answer with a
        // directory listing in "ls -l" format. writeCallback collects it in
        // "listing", parseDirectoryNames then picks the directories out of it.
        std::string listing;
        applyCommonOptions(url(requestPath));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listing);
        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            std::cerr << "Error reading " << requestPath << ": " << curl_easy_strerror(res) << "\n";
            return names;
        }
        return parseDirectoryNames(listing);
    }

    // NOTE the parameters: parent path and name, both as strings.
    bool makeDir(const std::string& parentPath, const std::string& name) {
        if (!curl) {
            std::cerr << "No open connection\n";
            return false;
        }
        std::string workingDirectory = parentPath;
        if (workingDirectory.empty() || workingDirectory.back() != '/') {
            workingDirectory += "/";
        }
        std::string newDirPath = joinPath(parentPath, name);
        return runQuoteCommand(workingDirectory, "MKD " + newDirPath, "Error creating " + newDirPath);
    }

    // NOTE the parameter: the complete path, as a string.
    bool removeDir(const std::string& fullPath) {
        if (!curl) {
            std::cerr << "No open connection\n";
            return false;
        }
        // The working directory for the call is the parent directory - curl must
        // not switch into the directory that is about to be deleted.
        std::string parentPath = fullPath.substr(0, fullPath.find_last_of('/') + 1);
        if (parentPath.empty()) {
            parentPath = "/";
        }
        return runQuoteCommand(parentPath, "RMD " + fullPath, "Error deleting " + fullPath);
    }
};

// ---------------------------------------------------------------------------
//  SFTP via libssh
//
//  Careful: SFTP is NOT "FTP with encryption". It is a completely different
//  protocol - a file transfer channel running INSIDE an SSH connection.
//  That is why connecting takes two steps:
//
//      1. ssh_*   - build the SSH connection (check host key, log in)
//      2. sftp_*  - open the SFTP channel on top of it
// ---------------------------------------------------------------------------

// A directory on the SFTP server. The FTP side above has no such class at all
// and passes plain strings around instead - that is one of the differences
// you will have to reconcile.
class SFTP_Directory {
    std::string path;

public:
    explicit SFTP_Directory(std::string path) : path(std::move(path)) {}
    std::string getPath() const { return path; }
};

class SFTP_Connection {
    std::string host;
    unsigned int port;
    std::string username;
    std::string password;
    bool acceptNewHostKey;
    ssh_session session = nullptr;
    sftp_session sftp = nullptr;

    // Fingerprint of the server key as a readable string.
    std::string serverFingerprint() const {
        ssh_key key = nullptr;
        if (ssh_get_server_publickey(session, &key) != SSH_OK) {
            return "<unreadable>";
        }
        unsigned char* hash = nullptr;
        size_t hashLength = 0;
        if (ssh_get_publickey_hash(key, SSH_PUBLICKEY_HASH_SHA256, &hash, &hashLength) != 0) {
            ssh_key_free(key);
            return "<unreadable>";
        }
        char* fingerprint = ssh_get_fingerprint_hash(SSH_PUBLICKEY_HASH_SHA256, hash, hashLength);
        std::string result = fingerprint ? fingerprint : "<unreadable>";
        ssh_string_free_char(fingerprint);
        ssh_clean_pubkey_hash(&hash);
        ssh_key_free(key);
        return result;
    }

    // ssh_connect() does NOT check the host key on its own - this must happen
    // explicitly, otherwise the SFTP connection is open to a man-in-the-middle attack.
    bool verifyHostKey() {
        switch (ssh_session_is_known_server(session)) {
            case SSH_KNOWN_HOSTS_OK:
                return true;
            case SSH_KNOWN_HOSTS_UNKNOWN:
            case SSH_KNOWN_HOSTS_NOT_FOUND:
                if (!acceptNewHostKey) {
                    std::cerr << "Unknown host key for " << host << " (" << serverFingerprint() << ")\n";
                    return false;
                }
                std::cout << "Unknown server " << host << "\n"
                          << "  Fingerprint: " << serverFingerprint() << "\n"
                          << "  Saving as trusted (Trust On First Use).\n"
                          << "  In real software, the fingerprint would have to be confirmed here!\n";
                if (ssh_session_update_known_hosts(session) != SSH_OK) {
                    std::cerr << "  Note: could not write known_hosts: "
                              << ssh_get_error(session) << "\n";
                }
                return true;
            case SSH_KNOWN_HOSTS_CHANGED:
                std::cerr << "WARNING: The host key of " << host << " has changed!\n"
                          << "  Fingerprint: " << serverFingerprint() << "\n"
                          << "  Possible man-in-the-middle attack - aborting connection.\n";
                return false;
            case SSH_KNOWN_HOSTS_OTHER:
                std::cerr << "WARNING: The server offers a different key type than the one stored.\n";
                return false;
            default:
                std::cerr << "Error during host key check: " << ssh_get_error(session) << "\n";
                return false;
        }
    }

public:
    SFTP_Connection(const std::string& host, unsigned int port, const std::string& username,
                    const std::string& password, bool acceptNewHostKey = true)
        : host(host), port(port), username(username), password(password), acceptNewHostKey(acceptNewHostKey) {}

    // The class owns ssh_session and sftp_session - see comment at FTP_Connection.
    SFTP_Connection(const SFTP_Connection&) = delete;
    SFTP_Connection& operator=(const SFTP_Connection&) = delete;
    SFTP_Connection(SFTP_Connection&&) = delete;
    SFTP_Connection& operator=(SFTP_Connection&&) = delete;

    ~SFTP_Connection() {
        disconnect();
    }

    // NOTE the return type: an int, 0 meaning success - modelled on libssh's
    // SSH_OK. The FTP class returns a bool, where TRUE means success.
    // So "if (ftp.open())" is correct, but "if (sftp.connect())" would be
    // exactly WRONG. A classic source of bugs.
    int connect() {
        disconnect();
        // Step 1: describe and open the SSH connection.
        session = ssh_new();
        if (!session) {
            std::cerr << "Could not create SSH session\n";
            return -1;
        }
        long timeout = connectTimeoutSeconds;
        ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str());
        ssh_options_set(session, SSH_OPTIONS_PORT, &port);
        ssh_options_set(session, SSH_OPTIONS_USER, username.c_str());
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
        if (ssh_connect(session) != SSH_OK) {
            std::cerr << "Connection failed: " << ssh_get_error(session) << "\n";
            disconnect();
            return -1;
        }
        if (!verifyHostKey()) {
            disconnect();
            return -1;
        }
        if (ssh_userauth_password(session, nullptr, password.c_str()) != SSH_AUTH_SUCCESS) {
            std::cerr << "Login failed: " << ssh_get_error(session) << "\n";
            disconnect();
            return -1;
        }
        // Step 2: only now, on top of the finished SSH connection, open the
        // actual SFTP channel.
        sftp = sftp_new(session);
        if (!sftp || sftp_init(sftp) != SSH_OK) {
            std::cerr << "SFTP init failed: " << ssh_get_error(session) << "\n";
            disconnect();
            return -1;
        }
        return 0;
    }

    int disconnect() {
        if (sftp) {
            sftp_free(sftp);
            sftp = nullptr;
        }
        if (session) {
            if (ssh_is_connected(session)) {
                ssh_disconnect(session);
            }
            ssh_free(session);
            session = nullptr;
        }
        return 0;
    }

    // NOTE: takes and returns OBJECTS, not strings. The objects already carry
    // their full path, so the caller does not have to assemble anything.
    std::vector<SFTP_Directory> getDirectories(const SFTP_Directory& directory) {
        std::vector<SFTP_Directory> directories;
        if (!sftp) {
            std::cerr << "No open connection\n";
            return directories;
        }
        sftp_dir dir = sftp_opendir(sftp, directory.getPath().c_str());
        if (!dir) {
            std::cerr << "Error reading " << directory.getPath() << ": " << ssh_get_error(session) << "\n";
            return directories;
        }
        sftp_attributes attributes = nullptr;
        while ((attributes = sftp_readdir(sftp, dir)) != nullptr) {
            const std::string name(attributes->name ? attributes->name : "");
            // attributes->type is always set; attributes->permissions only if the
            // server sends the SSH_FILEXFER_ATTR_PERMISSIONS flag.
            if (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY && name != "." && name != "..") {
                directories.push_back(SFTP_Directory(joinPath(directory.getPath(), name)));
            }
            sftp_attributes_free(attributes);
        }
        sftp_closedir(dir);
        return directories;
    }

    bool createDirectory(const SFTP_Directory& parent, const std::string& name) {
        if (!sftp) {
            std::cerr << "No open connection\n";
            return false;
        }
        std::string newDirPath = joinPath(parent.getPath(), name);
        if (sftp_mkdir(sftp, newDirPath.c_str(), 0755) != SSH_OK) {
            std::cerr << "Error creating " << newDirPath << ": " << ssh_get_error(session) << "\n";
            return false;
        }
        return true;
    }

    bool deleteDirectory(const SFTP_Directory& directory) {
        if (!sftp) {
            std::cerr << "No open connection\n";
            return false;
        }
        if (sftp_rmdir(sftp, directory.getPath().c_str()) != SSH_OK) {
            std::cerr << "Error deleting " << directory.getPath() << ": " << ssh_get_error(session) << "\n";
            return false;
        }
        return true;
    }
};

// ---------------------------------------------------------------------------
//  The two test runs
//
//  Read these two functions one after the other. They describe the SAME
//  sequence: list -> create -> list -> delete -> list.
//
//  And yet nothing can be shared between them, because every single line
//  differs in some detail: the method names, the return types, the parameter
//  types, and even the check for success. Even the heading printed at the top
//  has to be written twice.
//
//  main() runs both of them, and has to call each one by name. In
//  with_interface.cpp this whole block shrinks to ONE function runDemo(),
//  which main() then applies to both protocols in a single loop.
// ---------------------------------------------------------------------------

// The password is handed out separately (see README.md) and is
// deliberately not stored in this repository - paste it in before running.
const std::string serverPassword = "<password>";

// YOUR group name, e.g. "gruppe3". Every directory this program creates starts
// with it, so that the groups do not get in each other's way - do not simply
// take the one from the example, or you will be deleting each other's
// directories on the shared server.
const std::string groupPrefix = "";  // <-- enter your group name here

void runFtpDemo(const std::string& host, const std::string& newDirName) {
    std::cout << "\n=== FTP (port 21) ===\n";

    FTP_Connection connection(host, 21, "sftpuser", serverPassword);
    const std::string root = "/upload/";

    if (!connection.open()) {  // bool: true means success
        return;
    }

    std::cout << "Existing directories:\n";
    std::vector<std::string> names = connection.listDirectories(root);
    if (names.empty()) {
        std::cout << "  (none)\n";
    }
    for (const std::string& name : names) {
        // The caller has to build the full path itself.
        std::cout << "  " << joinPath(root, name) << "\n";
    }

    if (!connection.makeDir(root, newDirName)) {
        connection.close();
        return;
    }

    std::cout << "\nAfter creating:\n";
    names = connection.listDirectories(root);
    for (const std::string& name : names) {
        std::cout << "  " << joinPath(root, name) << "\n";
    }

    // Look for the new directory by NAME, because that is what we get here.
    bool found = false;
    for (const std::string& name : names) {
        if (name == newDirName) {
            found = true;
        }
    }
    if (!found) {
        std::cerr << "The created directory " << newDirName << " was not found\n";
        connection.close();
        return;
    }

    if (!connection.removeDir(joinPath(root, newDirName))) {
        connection.close();
        return;
    }

    std::cout << "\nAfter deleting:\n";
    names = connection.listDirectories(root);
    if (names.empty()) {
        std::cout << "  (none)\n";
    }
    for (const std::string& name : names) {
        std::cout << "  " << joinPath(root, name) << "\n";
    }

    connection.close();
}

void runSftpDemo(const std::string& host, const std::string& newDirName) {
    std::cout << "\n=== SFTP (port 22) ===\n";

    SFTP_Connection connection(host, 22, "sftpuser", serverPassword);
    const SFTP_Directory root("/upload/");

    if (connection.connect() != 0) {  // int: 0 means success!
        return;
    }

    std::cout << "Existing directories:\n";
    std::vector<SFTP_Directory> directories = connection.getDirectories(root);
    if (directories.empty()) {
        std::cout << "  (none)\n";
    }
    for (const SFTP_Directory& directory : directories) {
        // The object already knows its path.
        std::cout << "  " << directory.getPath() << "\n";
    }

    if (!connection.createDirectory(root, newDirName)) {
        connection.disconnect();
        return;
    }

    std::cout << "\nAfter creating:\n";
    directories = connection.getDirectories(root);
    for (const SFTP_Directory& directory : directories) {
        std::cout << "  " << directory.getPath() << "\n";
    }

    // Look for the new directory by OBJECT, and compare the last path element.
    const SFTP_Directory* created = nullptr;
    for (const SFTP_Directory& directory : directories) {
        if (baseName(directory.getPath()) == newDirName) {
            created = &directory;
        }
    }
    if (!created) {
        std::cerr << "The created directory " << newDirName << " was not found\n";
        connection.disconnect();
        return;
    }

    if (!connection.deleteDirectory(*created)) {
        connection.disconnect();
        return;
    }

    std::cout << "\nAfter deleting:\n";
    directories = connection.getDirectories(root);
    if (directories.empty()) {
        std::cout << "  (none)\n";
    }
    for (const SFTP_Directory& directory : directories) {
        std::cout << "  " << directory.getPath() << "\n";
    }

    connection.disconnect();
}

int main() {
    // Flush immediately so that messages to cout and cerr appear in the correct
    // order - otherwise cout is buffered when the output goes to a file.
    std::cout << std::unitbuf;

    // libcurl has to be initialised once globally before first use and
    // cleaned up again at the end.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    const std::string host = "son-ftp.cs.hshl.org";

    // groupPrefix and serverPassword are further up in this file - fill them
    // in there.
    if (groupPrefix.empty()) {
        std::cerr << "Please enter your group name in groupPrefix further up in"
                     " this file (see README.md).\n";
        curl_global_cleanup();
        return 1;
    }
    const std::string newDirName = groupPrefix + "_cpp_test";

    // Both protocols are tested, one after the other. Note how this has to be
    // written: two calls to two different functions, by name.
    //
    // It CANNOT be written as a loop over a list of connections, because there
    // is no type that both connections have in common - see the comment at the
    // end of this file. with_interface.cpp does exactly that loop, and that is
    // the difference the exercise is about.
    // (FTP uses port 21, SFTP port 22 - the ports are set inside the two
    // functions.)
    runFtpDemo(host, newDirName);
    runSftpDemo(host, newDirName);

    curl_global_cleanup();
    return 0;
}

/*
WHAT IS WRONG WITH THIS PROGRAM?

Functionally: nothing. It works. The problems only start when the program has
to be changed - and that is the normal case in software development.

1. Everything is written twice.
   runFtpDemo() and runSftpDemo() describe the same five steps. A bug fixed in
   one of them (say, the missing "(none)" line after creating) has to be
   remembered in the other one too.

2. The caller must know the concrete type.
   It is impossible to write a function
       void printAll(??? connection)
   that works for both, because there is no common type. Not even a template
   would help here: the method names themselves differ (open vs connect,
   listDirectories vs getDirectories).
   For the same reason main() cannot put the two connections into one
   std::vector and walk over it - it has to call both demo functions by name.

3. The different error conventions are dangerous.
       if (ftp.open())         // correct:   true  == success
       if (sftp.connect())     // WRONG:     0     == success
   Both compile. One of them is a bug.

4. A third protocol means touching everything.
   Adding e.g. FTPS means writing a third demo function and adapting every
   place that decides between the protocols.

THE FIX is to agree on a common set of methods - an INTERFACE - which both
classes implement. Then main() only talks to that interface and no longer
cares which protocol is behind it.

That is your task in with_interface.cpp.
*/
