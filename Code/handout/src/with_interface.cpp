/*
Exercise 02 - FTP/SFTP client, YOUR TASK (with interface)

Read without_interface.cpp first and run it. It works, but everything is
written twice, because FTP_Connection and SFTP_Connection have nothing in
common - not even the names of their methods.

Your job is to fix that with two INTERFACES.

WHAT YOU GET
    - Both classes, exactly as in without_interface.cpp (so you do not have to
      retype them). All the libcurl / libssh code inside them is finished and
      correct - you do not need to change how they talk to the server.
    - main() and listAndPrint() at the bottom of this file. They are ALREADY
      WRITTEN and you must not change them.

WHAT YOU HAVE TO DO
    main() is the specification. Look at how it uses the connection, and build
    the interfaces so that this code compiles and runs:

        ConnectionInterface& c = ...;
        c.connect();                            // bool, true == success
        c.getDirectories(someDirectory);        // vector<unique_ptr<DirectoryInterface>>
        c.createDirectory(parent, "name");      // bool
        c.deleteDirectory(someDirectory);       // bool
        c.disconnect();                         // bool

    1. Declare the interface DirectoryInterface with the method getPath().
    2. Declare the interface ConnectionInterface with the five methods above.
    3. Give the SFTP side an SFTP_Directory that implements DirectoryInterface.
    4. Invent an FTP_Directory - the FTP class currently has no directory class
       at all and works with plain strings.
    5. Make both connection classes implement ConnectionInterface. You will
       have to reconcile the differences:
           - open()/close()        must become connect()/disconnect()
           - the SFTP int (0 == OK) must become a bool (true == OK)
           - listDirectories() returning names must become getDirectories()
             returning DirectoryInterface objects
           - the FTP methods taking path strings must take DirectoryInterface

    The library code inside the methods stays as it is. You are only changing
    the OUTSIDE of the classes - the part the caller sees.

HINT
    This file does not compile at the start: main() uses types that do not
    exist yet. The compiler errors are your to-do list. Work through them from
    top to bottom. The other target, without_interface, keeps building and
    running the whole time, so you can always compare against it.
*/

#include <curl/curl.h>      // FTP
#include <libssh/libssh.h>  // SFTP: the SSH connection underneath
#include <libssh/sftp.h>    // SFTP: the file transfer channel on top of it

#include <iostream>
#include <memory>   // std::unique_ptr
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

// Last path component, e.g. "/upload/gruppe1" -> "gruppe1".
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

// ===========================================================================
//  TODO 1 - The interfaces
//
//  In C++ an interface is a class that contains only PURE VIRTUAL methods:
//
//      virtual std::string getPath() const = 0;
//
//  The "= 0" means: this class has no implementation of its own, every
//  derived class MUST provide one. Do not forget the virtual destructor -
//  otherwise deleting through a base pointer only destroys the base part.
//
//  Declare here:
//      class DirectoryInterface   -> getPath()
//      class ConnectionInterface  -> connect(), disconnect(), getDirectories(),
//                                    createDirectory(), deleteDirectory()
//
//  The exact signatures can be read off main() and listAndPrint() below.
// ===========================================================================

// ... your interfaces go here ...

// ===========================================================================
//  TODO 2 - A directory class for each protocol
//
//  Both only need to store a path and return it in getPath().
//  The SFTP version already exists further down in this file - move it up
//  here and make it implement DirectoryInterface. The FTP version does not
//  exist yet at all.
// ===========================================================================

// ... FTP_Directory and SFTP_Directory go here ...

// ---------------------------------------------------------------------------
//  FTP via libcurl
//
//  TODO 3: make this class implement ConnectionInterface.
//
//      class FTP_Connection : public ConnectionInterface { ... };
//
//  Everything below the "public:" line has to be adapted:
//      open()             -> connect()
//      close()            -> disconnect()
//      listDirectories()  -> getDirectories(), returning directory OBJECTS
//                            instead of names
//      makeDir()          -> createDirectory(), taking a DirectoryInterface
//                            instead of a path string
//      removeDir()        -> deleteDirectory(), likewise
//
//  Mark every implemented method with "override" - the compiler then tells
//  you when a signature does not match the interface.
//
//  The private part (writeCallback, url, applyCommonOptions, runQuoteCommand,
//  parseDirectoryNames) stays EXACTLY as it is.
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

// This class belongs further up, next to FTP_Directory, and has to implement
// DirectoryInterface (TODO 2).
class SFTP_Directory {
    std::string path;

public:
    explicit SFTP_Directory(std::string path) : path(std::move(path)) {}
    std::string getPath() const { return path; }
};

// ---------------------------------------------------------------------------
//  SFTP via libssh
//
//  TODO 4: same as TODO 3, for this class.
//      connect()/disconnect() must return bool instead of int
//      (careful: here 0 currently means SUCCESS!)
//      getDirectories() must return unique_ptr<DirectoryInterface> objects
//      instead of SFTP_Directory values.
//
//  The private part (serverFingerprint, verifyHostKey) stays as it is.
// ---------------------------------------------------------------------------
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

// ===========================================================================
//  GIVEN - do not change anything below this line.
//
//  This is the code that is supposed to work at the end. It is the
//  specification for your interfaces: every method it calls must exist in
//  ConnectionInterface / DirectoryInterface with exactly this signature.
//
//  Notice that neither listAndPrint() nor main() mentions FTP or SFTP
//  anywhere - except in the two marked lines where the object is created.
// ===========================================================================

// Note the parameter type: this function takes a ConnectionInterface&, so it
// works with an FTP connection just as well as with an SFTP one, without ever
// knowing which of the two it was given. That is polymorphism in practice.
//
// Lists the directories, prints them, and hands the list back for further use.
std::vector<std::unique_ptr<DirectoryInterface>> listAndPrint(ConnectionInterface& connection,
                                                             const DirectoryInterface& root,
                                                             const std::string& heading) {
    std::cout << heading << "\n";
    auto directories = connection.getDirectories(root);
    if (directories.empty()) {
        std::cout << "  (none)\n";
    }
    for (const auto& directory : directories) {
        std::cout << "  " << directory->getPath() << "\n";
    }
    return directories;
}

int main() {
    // Flush immediately so that messages to cout and cerr appear in the correct
    // order - otherwise cout is buffered when the output goes to a file.
    std::cout << std::unitbuf;

    // libcurl has to be initialised once globally before first use and
    // cleaned up again at the end.
    curl_global_init(CURL_GLOBAL_DEFAULT);

    // Own prefix so that the groups don't get in each other's way.
    const std::string groupPrefix = "gruppe1";
    const std::string newDirName = groupPrefix + "_cpp_test";

    // These two lines are the ONLY place where the concrete protocol appears.
    // For the FTP test, replace SFTP_Connection/SFTP_Directory with
    // FTP_Connection/FTP_Directory and change the port to 21 - everything
    // below stays exactly as it is, because it only uses the interface.
    // The password is handed out separately (see README.md) and is
    // deliberately not stored in this repository - paste it in before running.
    const std::string password = "<password>";

    SFTP_Connection connection("son-ftp.cs.hshl.org", 22, "sftpuser", password);
    SFTP_Directory root("/upload/");

    if (!connection.connect()) {
        curl_global_cleanup();
        return 1;
    }

    listAndPrint(connection, root, "Existing directories:");

    if (!connection.createDirectory(root, newDirName)) {
        connection.disconnect();
        curl_global_cleanup();
        return 1;
    }

    auto directories = listAndPrint(connection, root, "\nAfter creating:");

    // Find the created directory in the list instead of rebuilding the path -
    // this way the name can only be wrong in one place.
    const DirectoryInterface* created = nullptr;
    for (const auto& directory : directories) {
        if (baseName(directory->getPath()) == newDirName) {
            created = directory.get();
        }
    }
    if (!created) {
        std::cerr << "The created directory " << newDirName << " was not found\n";
        connection.disconnect();
        curl_global_cleanup();
        return 1;
    }

    if (!connection.deleteDirectory(*created)) {
        connection.disconnect();
        curl_global_cleanup();
        return 1;
    }

    listAndPrint(connection, root, "\nAfter deleting:");

    connection.disconnect();
    curl_global_cleanup();
    return 0;
}
