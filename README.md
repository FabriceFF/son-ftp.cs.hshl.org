# Übung 02 – FTP/SFTP-Client in C++

Kursmaterial für *Software Engineering*, WS 26/27 (HSHL).

**[Deutsch](#deutsch) · [English](#english)**

---

## Deutsch

### Einleitung

Sie **bekommen** eine funktionierende Software, die Dateien per SFTP und FTP auf einen Server überträgt. Sie kann bereits vorhandene Verzeichnisse auf einem Server auflisten, ein neues Verzeichnis anlegen und wieder löschen. Sie finden sie in `Code/handout` in der Datei `src/without_interface.cpp`. **Bauen Sie sie, führen Sie sie aus und lesen Sie sie, bevor Sie irgendetwas anderes tun.**

Das Programm funktioniert – aber es ist so entstanden, wie solche Dinge in der Realität oft wachsen: zwei Entwickler haben unabhängig voneinander je ein Protokoll implementiert. `FTP_Connection` und `SFTP_Connection` haben deshalb nichts gemeinsam, nicht einmal die Namen ihrer Methoden:

| `FTP_Connection` (libcurl) | `SFTP_Connection` (libssh) |
| --- | --- |
| `bool open()` | `int connect()` – Achtung: `0` bedeutet Erfolg |
| `bool close()` | `int disconnect()` |
| `std::vector<std::string> listDirectories(path)` | `std::vector<SFTP_Directory> getDirectories(dir)` |
| `bool makeDir(parentPath, name)` | `bool createDirectory(parent, name)` |
| `bool removeDir(fullPath)` | `bool deleteDirectory(dir)` |

Keines von beiden ist „falsch": jedes folgt der Bibliothek, die darunter liegt. Weil es aber keinen gemeinsamen Typ gibt, muss der Testlauf zweimal geschrieben werden (`runFtpDemo()` und `runSftpDemo()`), und nicht eine einzige Zeile lässt sich zwischen ihnen teilen. `main()` muss beide Funktionen namentlich aufrufen – die zwei Verbindungsobjekte haben keinen gemeinsamen Typ und lassen sich deshalb nicht einmal in eine gemeinsame Liste legen und in einer Schleife abarbeiten. Der Kommentarblock am Ende dieser Datei listet die Konsequenzen auf.

**Ihre Aufgabe** ist es, diese Dopplung zu beseitigen, indem Sie beide Klassen hinter ein gemeinsames Interface legen. Arbeiten Sie in der zweiten Datei, `src/with_interface.cpp`. Sie enthält bereits beide Klassen sowie eine fertige `main()` und die Testfunktion `runDemo()` – dieser vorgegebene Teil ist die Spezifikation, und Sie dürfen ihn nicht ändern. Beide Klassen sollen das Interface `Connection` implementieren (in C++ eine Klasse mit rein-virtuellen Methoden). Hierfür sind die folgenden Methoden vorgesehen:

- **connect** – Rückgabewert bool: Aufbau der Verbindung
- **disconnect** – Rückgabewert bool: Abbau der Verbindung
- **getDirectories** – Erhält als Übergabeparameter ein Verzeichnis-Objekt, für das die darin vorhandenen Verzeichnisse zurückgegeben werden sollen. Rückgabe einer Liste (z. B. `std::vector`) mit Directory-Objekten
- **createDirectory** – Erhält als Übergabeparameter (1) ein Verzeichnis-Objekt, in dem das Verzeichnis (Parameter 2), das als String übergeben wird, angelegt werden soll. Rückgabe bool
- **deleteDirectory** – Erhält als Übergabeparameter (1) ein Verzeichnis-Objekt, das gelöscht werden soll. Rückgabe bool

Die Übergabe der Verbindungsdaten soll im Konstruktor erfolgen.

Eine dieser Methoden ist in der Vorgabe **nicht** enthalten: `deleteDirectory` fehlt in beiden Verbindungsklassen und ist von Ihnen zu schreiben – an beiden Stellen steht ein `TODO 5`-Block, der beschreibt, was zu tun ist. Für FTP schicken Sie dafür das Kommando `RMD` (Achtung: als Arbeitsverzeichnis muss das *übergeordnete* Verzeichnis gesetzt sein, curl darf nicht in das Verzeichnis wechseln, das gerade gelöscht werden soll), für SFTP genügt `sftp_rmdir`. Die passenden Code-Schnipsel stehen weiter unten. Der übrige Bibliothekscode ist fertig; dort ändern Sie nur die Signaturen.

Wie Sie der Beschreibung entnehmen können, sollen die Verzeichnisse als Objekte zurückgegeben werden. Für FTP-Verzeichnisse sollen Objekte vom Typ `FTP_Directory` und für SFTP-Verzeichnisse Objekte vom Typ `SFTP_Directory` zurückgegeben werden. Beide Objekte sollen das Interface `Directory` implementieren. Das Interface `Directory` stellt die Methode `getPath` (Rückgabe Pfad als String) zur Verfügung, der Pfad soll über den Konstruktor gesetzt werden.

Als Bibliotheken verwenden wir **libcurl** für FTP und **libssh** für SFTP. (Hinweis: Die unter Ubuntu mit libcurl mitgelieferte SFTP-Unterstützung kann keine Verzeichnisse anlegen/löschen, daher hier der direkte Griff zur libssh.)

Diese Abfolge – die vorhandenen Ordner auflisten, ein neues Verzeichnis anlegen, erneut auflisten, das angelegte Verzeichnis löschen, ein letztes Mal auflisten – ist in der Vorgabe bereits implementiert, und zwar genau **einmal**: als Funktion `runDemo()`, die nur die beiden Interfaces kennt. `main()` legt eine FTP- und eine SFTP-Verbindung in denselben `std::vector` und lässt `runDemo()` in einer Schleife über beide laufen – ein Durchlauf testet also beide Protokolle.

Genau darin liegt der Gewinn: was in `without_interface.cpp` zwei Funktionen sind, ist hier eine. Ihre Aufgabe ist es, beide Verbindungsklassen dazu passend zu machen, sodass diese Schleife unverändert kompiliert und läuft. Weil beide Protokolle durch dieselbe Schleife laufen, genügt es nicht, nur eines von beiden fertigzustellen.

### Aufgabenstellung

1. Recherchieren Sie: Was sind FTP und SFTP? Was sind die Unterschiede der Vorgehen? Was sind Vor- bzw. Nachteile der jeweiligen Vorgehen?
2. Recherchieren Sie zusätzlich die Begriffe, die Sie nicht kennen (z. B. cURL, SSH-Hostkey, Chroot, reine virtuelle Methode).
3. Probieren Sie beide Protokolle zuerst mit einem fertigen Client aus: **WinSCP** (Windows, kostenlos) beherrscht SFTP *und* FTP. Legen Sie eine Sitzung mit den Zugangsdaten von unten an – einmal mit Protokoll „SFTP" auf Port 22, einmal mit „FTP" auf Port 21 – und legen Sie in `/upload` probeweise ein Verzeichnis an und löschen es wieder. So sehen Sie, was Ihr Programm später selbst tun soll, Verbindungsprobleme fallen sofort auf (und nicht erst im eigenen Code), und während der Entwicklung können Sie jederzeit nachsehen, ob Ihre Verzeichnisse wirklich angelegt bzw. gelöscht wurden. (Unter Linux/macOS tun es `sftp` und `ftp` auf der Kommandozeile oder FileZilla.)
4. Überlegen Sie sich, wie Sie sinnvolle Arbeitspakete schnüren. Beispiel: Es macht vielleicht keinen Sinn, zunächst alle Klassen zu bauen und dann an der Verbindung zu scheitern. Denken Sie an „Divide and Conquer"!
5. Refaktorieren Sie die vorgegebene Software gleichberechtigt im Team: Entwerfen Sie die beiden Interfaces und passen Sie beide Klassen daran an. Am Ende soll die Testsequenz nur noch einmal im Code stehen und in einer Schleife für beide Protokolle durchlaufen werden.
6. Implementieren Sie `deleteDirectory` in beiden Verbindungsklassen selbst – diese Methode ist als einzige nicht vorgegeben (FTP: `RMD`, SFTP: `sftp_rmdir`).
7. Reflektieren Sie die Partnerarbeit: Was hat gut geklappt? Was war weniger gut?
8. Bewerten Sie das Vorgehen: Halten Sie solche Textaufgaben für die Softwareentwicklung für sinnvoll?

### Build

Das Projekt `Code/handout` baut **zwei** Programme; in CLion wechseln Sie oben rechts im Run-Configuration-Dropdown zwischen ihnen:

- `without_interface` – der vorgegebene Ausgangspunkt. Baut und läuft sofort.
- `with_interface` – Ihre Aufgabe. Dieses Programm **kompiliert zu Beginn nicht**, weil `main()`
  die Interfaces benutzt, die Sie erst noch schreiben müssen. Die Compilerfehler sind Ihre
  To-do-Liste; arbeiten Sie sie von oben nach unten ab.

**Windows (CLion):** Öffnen Sie den Ordner `Code/handout` in CLion und warten Sie den CMake-Reload ab. Es muss nichts installiert werden: CMake lädt libcurl und libssh beim ersten Lauf herunter und baut sie zusammen mit dem Projekt. Der erste Build dauert deshalb einige Minuten, jeder weitere ist schnell.

**Ubuntu:** Die Bibliotheken einmalig installieren, dann bauen:

```bash
sudo apt install libcurl4-openssl-dev libssh-dev cmake g++
cmake -B build Code/handout && cmake --build build --target without_interface
./build/without_interface
```

### Wichtige Hinweise

- Lesen Sie diese Aufgabenstellung sorgfältig.
- Tragen Sie Ihren Gruppennamen in die Konstante `groupPrefix` am Anfang der Quelldatei ein
  (z. B. `gruppe3`). Sie ist absichtlich leer – ohne Eintrag bricht das Programm sofort mit
  einer Meldung ab. Alle angelegten Ordner beginnen damit, so entstehen keine Konflikte mit
  anderen Gruppen. Nehmen Sie deshalb **nicht** den Namen aus einem Beispiel, sondern Ihren
  eigenen.
- Der Upload-Ordner `/upload` ist als RAM-basiertes Dateisystem (tmpfs, 100 MB) eingerichtet:
  Er kann nicht „voll" laufen, aber sein Inhalt ist nach einem Neustart des Servers gelöscht.
  Wichtige Ergebnisse sollten Sie also lokal speichern.
- Beide Programme testen **beide** Protokolle in einem Durchlauf, zuerst FTP (Port 21), dann
  SFTP (Port 22). Der Unterschied liegt darin, *wie*: `without_interface` ruft dafür zwei
  getrennte Funktionen (`runFtpDemo()`, `runSftpDemo()`) nacheinander auf, `with_interface`
  läuft mit einer Schleife über einen `std::vector` mit beiden Verbindungen.
- Scheitert ein Protokoll (z. B. weil der FTP-Datenport blockiert ist), läuft das andere
  trotzdem; die Schleife bricht nicht ab. `main()` liefert dann den Exit-Code 1.

### Repository-Aufbau

| Pfad | Was es ist |
| --- | --- |
| `Code/handout/src/without_interface.cpp` | Ausgangspunkt: funktioniert, aber alles ist doppelt geschrieben |
| `Code/handout/src/with_interface.cpp` | Ihre Arbeitsdatei: `main()` und `runDemo()` sind vorgegeben; die Interfaces und `deleteDirectory` fehlen |
| `Code/handout/diagrams/` | draw.io-Diagramme: Laufzeitverhalten und der jetzige Entwurf ohne Interfaces |
| `Code/handout/CMakeLists.txt` | Build-Definition für beide Programme |

### Zugangsdaten

Das Passwort ist **nicht** Teil dieses Repositorys. Es wird separat über Moodle ausgegeben, in der Datei `credentials.local.md`. Dieser Dateiname steht in der `.gitignore` und kann deshalb nicht versehentlich eingecheckt werden.

Alles Übrige ist öffentlich:

```
Host:      son-ftp.cs.hshl.org
Username:  sftpuser
Port FTP:  21
Port SFTP: 22
```

Tragen Sie das Passwort in die Konstante `serverPassword` der Datei ein, an der Sie gerade arbeiten (`Code/handout/src/*.cpp`) – sie steht zusammen mit `groupPrefix` am Anfang der Datei. Checken Sie das Passwort nicht ein.

### Code-Schnipsel

#### FTP (libcurl)

Bibliothek einbinden:

```cpp
#include <curl/curl.h>
```

Verbindung aufbauen und Daten setzen:

```cpp
curl_global_init(CURL_GLOBAL_DEFAULT);
CURL* curl = curl_easy_init();

std::string url  = "ftp://son-ftp.cs.hshl.org:21/upload/";
std::string user = "sftpuser:<password>";  // siehe credentials.local.md
curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
```

Inhalt eines Verzeichnisses erhalten: Ohne weitere Optionen liefert ein GET auf eine Verzeichnis-URL die Ausgabe im `ls -l`-Format. Zeilen, die mit `d` beginnen, sind Verzeichnisse; der Name steht nach dem 8. Feld.

```cpp
auto writeCallback = [](void* contents, size_t size, size_t count, void* userp) -> size_t {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * count);
    return size * count;
};

std::string listing;
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listing);
CURLcode res = curl_easy_perform(curl);
```

Verzeichnis erstellen (`QUOTE` führt ein Kommando direkt auf dem Server aus, `CURLOPT_NOBODY` unterdrückt die Datenübertragung):

```cpp
struct curl_slist* quote = curl_slist_append(nullptr, "MKD /upload/<gruppe>_neu");
curl_easy_setopt(curl, CURLOPT_URL, "ftp://son-ftp.cs.hshl.org:21/upload/");
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
curl_easy_setopt(curl, CURLOPT_QUOTE, quote);
CURLcode res = curl_easy_perform(curl);
curl_slist_free_all(quote);
```

Verzeichnis löschen:

```cpp
struct curl_slist* quote = curl_slist_append(nullptr, "RMD /upload/<gruppe>_neu");
curl_easy_setopt(curl, CURLOPT_URL, "ftp://son-ftp.cs.hshl.org:21/upload/");
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
curl_easy_setopt(curl, CURLOPT_QUOTE, quote);
CURLcode res = curl_easy_perform(curl);
curl_slist_free_all(quote);
```

#### SFTP (libssh)

Bibliotheken einbinden:

```cpp
#include <libssh/libssh.h>
#include <libssh/sftp.h>
```

Verbindung herstellen:

```cpp
ssh_session session = ssh_new();
unsigned int port = 22;
ssh_options_set(session, SSH_OPTIONS_HOST, "son-ftp.cs.hshl.org");
ssh_options_set(session, SSH_OPTIONS_PORT, &port);
ssh_options_set(session, SSH_OPTIONS_USER, "sftpuser");
ssh_connect(session);
ssh_userauth_password(session, nullptr, "<password>");  // siehe credentials.local.md

sftp_session sftp = sftp_new(session);
sftp_init(sftp);
```

**Achtung – Hostkey prüfen:** `ssh_connect()` prüft den Hostkey des Servers *nicht* von selbst. Ohne diese Prüfung ist die Verbindung zwar verschlüsselt, Sie wissen aber nicht, mit *wem* – genau das ist die Lücke, die ein Man-in-the-Middle-Angriff ausnutzt. Zwischen `ssh_connect()` und `ssh_userauth_password()` gehört daher:

```cpp
switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        break;                                  // Schlüssel bekannt und unverändert
    case SSH_KNOWN_HOSTS_UNKNOWN:
    case SSH_KNOWN_HOSTS_NOT_FOUND:
        // Server noch nie gesehen: Fingerprint anzeigen, bestätigen lassen, dann speichern
        ssh_session_update_known_hosts(session);
        break;
    case SSH_KNOWN_HOSTS_CHANGED:
        std::cerr << "Hostkey hat sich geaendert - Verbindung abbrechen!\n";
        return false;
    default:
        return false;
}
```

Den Fingerprint zum Anzeigen liefern `ssh_get_server_publickey()`, `ssh_get_publickey_hash()` und `ssh_get_fingerprint_hash()` – siehe libssh-Dokumentation.

In ein Verzeichnis wechseln / Inhalt eines Verzeichnisses erhalten (nur Verzeichnisse ausgeben):

```cpp
sftp_dir dir = sftp_opendir(sftp, "/upload/");
sftp_attributes attributes;
while ((attributes = sftp_readdir(sftp, dir)) != nullptr) {
    if (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY && std::string(attributes->name) != "." && std::string(attributes->name) != "..") {
        std::cout << attributes->name << "\n";
    }
    sftp_attributes_free(attributes);
}
sftp_closedir(dir);
```

(`attributes->type` ist immer gesetzt. Das naheliegende `S_ISDIR(attributes->permissions)` funktioniert nur, solange der Server das Flag `SSH_FILEXFER_ATTR_PERMISSIONS` mitschickt – verlassen Sie sich nicht darauf.)

Verzeichnis erstellen:

```cpp
sftp_mkdir(sftp, "/upload/<gruppe>_neu", 0755);
```

Verzeichnis löschen:

```cpp
sftp_rmdir(sftp, "/upload/<gruppe>_neu");
```

Verbindung schließen:

```cpp
sftp_free(sftp);
ssh_disconnect(session);
ssh_free(session);
```

### Hinweis zur FTP-Erreichbarkeit

SFTP (Port 22) und FTP (Port 21) funktionieren beide von außen und sind getestet. Für FTP ist zusätzlich der Passiv-Datenportbereich 40000–40100 freigeschaltet, sodass auch mehrere Gruppen gleichzeitig arbeiten können.

Für Aufgabe 1 lohnt es sich zu verstehen, warum ausgerechnet FTP diesen Ärger macht: FTP benutzt zwei getrennte TCP-Verbindungen – über Port 21 laufen nur Kommandos, während jedes Verzeichnis-Listing und jede Übertragung eine zweite Verbindung auf einem eigenen Port öffnet. Deshalb brauchen Login, `createDirectory` und `deleteDirectory` nur den Steuerkanal, `getDirectories` dagegen zusätzlich einen freigegebenen Datenport. SFTP multiplext alles über die eine SSH-Verbindung auf Port 22 und kommt genau deshalb unverändert durch jede Firewall.

---

## English

### Introduction

You are **given** a working piece of software that transfers files to a server via SFTP and FTP. It can already list the existing directories on a server, create a new directory and delete it again. You will find it in `Code/handout` in the file `src/without_interface.cpp`. **Build it, run it and read it before you do anything else.**

The program works – but it was written the way such things often grow in reality: two developers implemented one protocol each, independently of one another. `FTP_Connection` and `SFTP_Connection` therefore have nothing in common, not even the names of their methods:

| `FTP_Connection` (libcurl) | `SFTP_Connection` (libssh) |
| --- | --- |
| `bool open()` | `int connect()` – careful: `0` means success |
| `bool close()` | `int disconnect()` |
| `std::vector<std::string> listDirectories(path)` | `std::vector<SFTP_Directory> getDirectories(dir)` |
| `bool makeDir(parentPath, name)` | `bool createDirectory(parent, name)` |
| `bool removeDir(fullPath)` | `bool deleteDirectory(dir)` |

Neither of the two is "wrong": each one follows the library underneath it. But because there is no common type, the test run has to be written twice (`runFtpDemo()` and `runSftpDemo()`), and not a single line can be shared between them. `main()` has to call both functions by name – the two connection objects have no common type, so they cannot even be put into one list and walked over in a loop. The comment block at the end of that file lists the consequences.

**Your task** is to remove that duplication by putting both classes behind a common interface. Work in the second file, `src/with_interface.cpp`. It already contains both classes as well as a finished `main()` and the test function `runDemo()` – that given part is the specification, and you must not change it. Both classes should implement the interface `Connection` (in C++, a class with pure-virtual methods). The following methods are foreseen for this purpose:

- **connect** – return value bool: establish the connection
- **disconnect** – return value bool: tear down the connection
- **getDirectories** – receives as parameter a directory object for which the directories contained within it are to be returned. Returns a list (e.g. `std::vector`) of Directory objects
- **createDirectory** – receives as parameter (1) a directory object in which the directory (parameter 2), passed as a string, is to be created. Returns bool
- **deleteDirectory** – receives as parameter (1) a directory object that is to be deleted. Returns bool

The connection data should be passed in the constructor.

One of these methods is **not** part of the handout: `deleteDirectory` is missing in both connection classes and is for you to write – a `TODO 5` block in each class says what is needed. For FTP you send the command `RMD` (careful: the working directory of that call has to be the *parent* directory, curl must not change into the directory that is about to be deleted), for SFTP `sftp_rmdir` is enough. The matching code snippets are further down. The rest of the library code is finished; there you only change the signatures.

As you can gather from the description, the directories are to be returned as objects. For FTP directories, objects of type `FTP_Directory` and for SFTP directories objects of type `SFTP_Directory` should be returned. Both objects should implement the interface `Directory`. The interface `Directory` provides the method `getPath` (returns the path as a string); the path should be set via the constructor.

We use the libraries **libcurl** for FTP and **libssh** for SFTP. (Note: the SFTP support shipped with libcurl under Ubuntu cannot create/delete directories, hence going directly to libssh here.)

That sequence – list the existing folders, create a new directory, list again, delete the created directory, list once more – is already implemented for you, and exactly **once**: as the function `runDemo()`, which knows nothing but the two interfaces. `main()` puts one FTP and one SFTP connection into the same `std::vector` and runs `runDemo()` over both of them in a loop, so a single run tests both protocols.

That is where the payoff is: what takes two functions in `without_interface.cpp` is one function here. Your job is to make both connection classes fit it, so that this loop compiles and runs unchanged. And because both protocols are driven by the same loop, finishing only one of them is not enough.

### Assignment

1. Do some research: What are FTP and SFTP? What are the differences between the approaches? What are the advantages and disadvantages of the respective approaches?
2. Also look up any terms you do not know (e.g. cURL, SSH host key, chroot, pure virtual method).
3. Try both protocols with a ready-made client first: **WinSCP** (Windows, free of charge) speaks SFTP *and* FTP. Set up a session with the credentials below – once with protocol "SFTP" on port 22, once with "FTP" on port 21 – and create and delete a directory in `/upload` by hand. That shows you what your program is supposed to do later, connection problems show up right away (instead of inside your own code), and during development you can always check whether your directories really were created or deleted. (On Linux/macOS the command line `sftp` and `ftp`, or FileZilla, do the same job.)
4. Think about how to define sensible work packages. Example: it probably makes no sense to build all the classes first and then fail on the connection. Remember "Divide and Conquer"!
5. Refactor the supplied software as equal partners in the team: design the two interfaces and adapt both classes to them. In the end the test sequence should appear only once in the code and be run for both protocols in a loop.
6. Implement `deleteDirectory` in both connection classes yourself – it is the only method that is not given (FTP: `RMD`, SFTP: `sftp_rmdir`).
7. Reflect on the pair work: What worked well? What was less good?
8. Evaluate the approach: do you consider such text-based exercises sensible for software development?

### Build

The project `Code/handout` builds **two** programs; in CLion you switch between them in the
run configuration drop-down at the top right:

- `without_interface` – the given starting point. Builds and runs immediately.
- `with_interface` – your task. This one **does not compile at the start**, because `main()`
  uses the interfaces you still have to write. The compiler errors are your to-do list; work
  through them from top to bottom.

**Windows (CLion):** open the folder `Code/handout` in CLion and wait for the CMake reload.
Nothing has to be installed: CMake downloads libcurl and libssh on the first run and builds
them together with the project. The first build therefore takes a few minutes, every
following one is fast.

**Ubuntu:** install the libraries once, then build:

```bash
sudo apt install libcurl4-openssl-dev libssh-dev cmake g++
cmake -B build Code/handout && cmake --build build --target without_interface
./build/without_interface
```

### Important Notes

- Read this assignment carefully.
- Enter your group name in the constant `groupPrefix` at the top of the source file (e.g.
  `gruppe3`). It is deliberately empty – without it the program stops right away with a
  message. Every folder the program creates starts with it, so no conflicts arise with other
  groups. Do **not** take the name from an example, use your own.
- The upload folder `/upload` is set up as a RAM-based filesystem (tmpfs, 100 MB): it cannot
  run "full", but its content is deleted after a server restart. You should therefore save
  important results locally.
- Both programs test **both** protocols in a single run, first FTP (port 21), then SFTP
  (port 22). The difference is *how*: `without_interface` calls two separate functions
  (`runFtpDemo()`, `runSftpDemo()`) one after the other, while `with_interface` loops over a
  `std::vector` holding both connections.
- If one protocol fails (e.g. because the FTP data port is blocked), the other one still
  runs; the loop does not abort. `main()` then returns exit code 1.

### Repository Layout

| Path | What it is |
| --- | --- |
| `Code/handout/src/without_interface.cpp` | Starting point: works, but everything is written twice |
| `Code/handout/src/with_interface.cpp` | Your working file: `main()` and `runDemo()` are given; the interfaces and `deleteDirectory` are missing |
| `Code/handout/diagrams/` | draw.io diagrams: the runtime behaviour and the current design without interfaces |
| `Code/handout/CMakeLists.txt` | Build definition for both programs |

### Credentials

The password is **not** part of this repository. It is handed out separately in Moodle, in
the file `credentials.local.md`. That file name is listed in `.gitignore`, so it cannot be
committed by accident.

Everything else you need is public:

```
Host:      son-ftp.cs.hshl.org
Username:  sftpuser
Port FTP:  21
Port SFTP: 22
```

Paste the password into the `serverPassword` constant in the source file you are working on
(`Code/handout/src/*.cpp`) – it sits next to `groupPrefix` at the top of the file. Do not
commit the password.

### Code Snippets

#### FTP (libcurl)

Include the library:

```cpp
#include <curl/curl.h>
```

Establish the connection and set the data:

```cpp
curl_global_init(CURL_GLOBAL_DEFAULT);
CURL* curl = curl_easy_init();

std::string url  = "ftp://son-ftp.cs.hshl.org:21/upload/";
std::string user = "sftpuser:<password>";  // see credentials.local.md
curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
```

Get the content of a directory: without further options, a GET on a directory URL returns the output in `ls -l` format. Lines that begin with `d` are directories; the name comes after the 8th field.

```cpp
auto writeCallback = [](void* contents, size_t size, size_t count, void* userp) -> size_t {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * count);
    return size * count;
};

std::string listing;
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &listing);
CURLcode res = curl_easy_perform(curl);
```

Create a directory (`QUOTE` executes a command directly on the server, `CURLOPT_NOBODY` suppresses the data transfer):

```cpp
struct curl_slist* quote = curl_slist_append(nullptr, "MKD /upload/<gruppe>_neu");
curl_easy_setopt(curl, CURLOPT_URL, "ftp://son-ftp.cs.hshl.org:21/upload/");
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
curl_easy_setopt(curl, CURLOPT_QUOTE, quote);
CURLcode res = curl_easy_perform(curl);
curl_slist_free_all(quote);
```

Delete a directory:

```cpp
struct curl_slist* quote = curl_slist_append(nullptr, "RMD /upload/<gruppe>_neu");
curl_easy_setopt(curl, CURLOPT_URL, "ftp://son-ftp.cs.hshl.org:21/upload/");
curl_easy_setopt(curl, CURLOPT_USERPWD, user.c_str());
curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
curl_easy_setopt(curl, CURLOPT_QUOTE, quote);
CURLcode res = curl_easy_perform(curl);
curl_slist_free_all(quote);
```

#### SFTP (libssh)

Include the libraries:

```cpp
#include <libssh/libssh.h>
#include <libssh/sftp.h>
```

Establish the connection:

```cpp
ssh_session session = ssh_new();
unsigned int port = 22;
ssh_options_set(session, SSH_OPTIONS_HOST, "son-ftp.cs.hshl.org");
ssh_options_set(session, SSH_OPTIONS_PORT, &port);
ssh_options_set(session, SSH_OPTIONS_USER, "sftpuser");
ssh_connect(session);
ssh_userauth_password(session, nullptr, "<password>");  // see credentials.local.md

sftp_session sftp = sftp_new(session);
sftp_init(sftp);
```

**Attention – check the host key:** `ssh_connect()` does *not* check the server's host key on its own. Without this check the connection is encrypted, but you don't know *with whom* – this is exactly the gap that a man-in-the-middle attack exploits. Therefore the following belongs between `ssh_connect()` and `ssh_userauth_password()`:

```cpp
switch (ssh_session_is_known_server(session)) {
    case SSH_KNOWN_HOSTS_OK:
        break;                                  // key known and unchanged
    case SSH_KNOWN_HOSTS_UNKNOWN:
    case SSH_KNOWN_HOSTS_NOT_FOUND:
        // Server never seen before: display fingerprint, have it confirmed, then save it
        ssh_session_update_known_hosts(session);
        break;
    case SSH_KNOWN_HOSTS_CHANGED:
        std::cerr << "Host key has changed - aborting connection!\n";
        return false;
    default:
        return false;
}
```

The fingerprint for display is provided by `ssh_get_server_publickey()`, `ssh_get_publickey_hash()` and `ssh_get_fingerprint_hash()` – see the libssh documentation.

Change into a directory / get the content of a directory (list only directories):

```cpp
sftp_dir dir = sftp_opendir(sftp, "/upload/");
sftp_attributes attributes;
while ((attributes = sftp_readdir(sftp, dir)) != nullptr) {
    if (attributes->type == SSH_FILEXFER_TYPE_DIRECTORY && std::string(attributes->name) != "." && std::string(attributes->name) != "..") {
        std::cout << attributes->name << "\n";
    }
    sftp_attributes_free(attributes);
}
sftp_closedir(dir);
```

(`attributes->type` is always set. The obvious `S_ISDIR(attributes->permissions)` only works as long as the server sends the `SSH_FILEXFER_ATTR_PERMISSIONS` flag – don't rely on it.)

Create a directory:

```cpp
sftp_mkdir(sftp, "/upload/<gruppe>_neu", 0755);
```

Delete a directory:

```cpp
sftp_rmdir(sftp, "/upload/<gruppe>_neu");
```

Close the connection:

```cpp
sftp_free(sftp);
ssh_disconnect(session);
ssh_free(session);
```

### Note on FTP reachability

SFTP (port 22) and FTP (port 21) both work from outside and have been tested. For FTP the
passive data port range 40000-40100 is open as well, so several groups can work at the same
time.

It is worth understanding for task 1 why FTP of all things causes this trouble: FTP uses two
separate TCP connections – port 21 carries only commands, while every directory listing or
transfer opens a second connection on its own port. That is why login, `createDirectory` and
`deleteDirectory` need only the control channel, while `getDirectories` also needs an open
data port. SFTP multiplexes everything over the single SSH connection on port 22, which is exactly
why it passes any firewall unchanged.
