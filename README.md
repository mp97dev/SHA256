# SHA256 - Michele Pasetto VR495361

## Introduzione

Il progetto si prospetta l'implementazione di un servizio client-server per la computazione dell'hash SHA-256 di un file. L'architettura si basa su comunicazione tramite FIFO, gestione concorrente delle richieste mediante thread e cache condivisa, per ottimizzare le prestazioni ed una coda ordinata per dimensione del file.

L'obiettivo principale è dimostrare l'utilizzo efficace di:
- **Comunicazione interprocesso** tramite FIFO
- **Sincronizzazione** con mutex e variabili di condizione
- **Thread** per gestione concorrente
- **Cache** per evitare calcoli ridondanti
- **Schedulazione prioritaria** basata sulla dimensione del file

## Contenuto dei file
La struttura delle cartelle è la seguente
```bash
.
├── CMakeLists.txt
├── README.md
├── dist
├── include
│   ├── cache.h
│   ├── common.h
│   ├── queue.h
│   └── sha256_utils.h
├── run_test.sh
├── script
│   └── lib.sh
├── src
│   ├── cache.c
│   ├── client.c
│   ├── queue.c
│   ├── server.c
│   └── sha256_utils.c
└── test
    ├── emptyfile.txt
    ├── password.txt
    └── testfile.txt
```

| path | description |
|:-:|:-:|
| `dist` | Viene usata per inserire i compilati|
| `include` | Contiene gli header files (.h) con definizioni di strutture e prototipi |
| `script` | Contiene script bash utili per la compilazione e il testing |
| `src` | Contiene i codici sorgente (.c) del progetto |
| `test` | Contiene i file di test usati per debug e unit tests |

### Descrizione dei file principali

#### File sorgente (src/)
- **client.c**: Implementa il client che invia richieste di hash al server tramite FIFO
- **server.c**: Implementa il server multi-thread che gestisce le richieste
- **queue.c**: Implementa la coda prioritaria thread-safe ordinata per dimensione file
- **cache.c**: Implementa la cache condivisa con sincronizzazione per evitare calcoli duplicati
- **sha256_utils.c**: Funzioni di utilità per calcolo SHA-256 usando OpenSSL

#### File header (include/)
- **common.h**: Definizioni comuni (strutture dati, costanti, macro) usate da client e server
- **queue.h**: Interfaccia della coda prioritaria
- **cache.h**: Interfaccia della cache condivisa
- **sha256_utils.h**: Prototipo per calcolo SHA-256

## Requisiti

### Librerie necessarie
- **OpenSSL** (libssl-dev): per il calcolo dell'hash SHA-256
- **pthread**: per gestione thread pool e sincronizzazione
- **CMake** (>= 3.10): per compilazione automatica

### Installazione dipendenze
```bash
# Ubuntu/Debian
sudo apt-get install libssl-dev cmake build-essential
```

## Compilazione

Il progetto utilizza CMake per la compilazione automatica:

```bash
mkdir -p dist
cd dist
cmake ..
make
```

Questo produce due eseguibili:
- `dist/server`: il server multi-thread
- `dist/client`: il client

## Esecuzione

### Avvio del server
Il server deve essere avviato prima dei client:

```bash
$ cd dist
$ ./server
>
[SERVER] Listening on /tmp/sha256_req_fifo
```

### Esecuzione del client

#### Richiesta di hash per un file
```bash
$ ./client path/to/file.txt
>
SHA-256: a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3
```

#### Query della cache
```bash
./client CACHE?
```

Il server stamperà su stdout il contenuto corrente della cache.
## Testing

### Script di test automatico

Lo script `run_test.sh` esegue automaticamente:
1. Compilazione dei file sorgenti
2. Avvio del server in background
3. Esecuzione dei test del client usando i file nella cartella `/test` 
4. Verifica correttezza hash tramite confronto con `sha256sum`
5. Terminazione del server

Esecuzione:
```bash
$ ./run_test.sh test/testfile.txt
>
[➡] CMake building
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /home/ginkgo/VR495361_Pasetto_Michele_sha256_service/dist
[✔] dist completed
[➡] Starting server

[ℹ] Server started with PID 7384
[SERVER] Listening on /tmp/sha256_req_fifo
[➡] Running client tests for multiple files

[ℹ] Submitting file to server: test/testfile.txt
    SHA-256: a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3
[✔] Digest matches CLI
[✔] All tests completed successfully
```

### Test di concorrenza

Per testare richieste concorrenti:
```bash
# In terminali separati dopo aver avviato il server
./dist/client test/testfile.txt &
./dist/client test/password.txt &
./dist/client test/emptyfile.txt &
```

Il sistema gestisce correttamente:
- **Richieste concorrenti** per file diversi (parallelismo tramite thread pool)
- **Richieste duplicate** per lo stesso file (sincronizzazione tramite cache)

## Dettagli implementativi

### Architettura del sistema

#### Thread Pool (server.c)
Un pool di **4 thread worker** sempre attivi permette ad ogni thread di estrarre la richieste dalla coda prioritaria.

#### Coda prioritaria (queue.c)
La coda è una lista ordinata per dimensione del file, dando priorità ai lavori "leggeri", ovvero i file di dimensioni ridotte. Un mutex e una condition variable gestiscono i thread. `queue_push()` inserisce il job al posto giusto, `queue_pop()` aspetta finché c’è qualcosa e poi lo consegna al primo thread libero.

#### Cache condivisa (cache.c)
- Lista concatenata di entry con **mutex e condition variable per entry**
- `cache_lookup_or_insert()`: cerca o crea entry atomicamente
- `cache_set_digest()`: aggiorna risultato e notifica thread in attesa
- Permette a thread multipli di attendere lo stesso calcolo

#### Comunicazione FIFO
- **FIFO request**: `/tmp/sha256_req_fifo` (globale, write-only per client)
- **FIFO response**: `/tmp/sha256_resp_<PID>_fifo` (per client, create dal client stesso)

#### Calcolo SHA-256 (sha256_utils.c)
- Usa API OpenSSL: `SHA256_Init()`, `SHA256_Update()`, `SHA256_Final()`
- Lettura file a blocchi di 4096 byte
- Conversione digest binario (32 byte) in stringa hex (64 caratteri)

### Gestione errori
- Controllo apertura file e FIFO
- Verifica lettura/scrittura completa
- Log errori
- Status code nelle risposte: 0=success, 1=error, 3=cache hit

## Note

### Scelte progettuali
- **FIFO in /tmp**: facilita l'eliminazione di file temporanei generati durante l'esecuzione
- **Client rimuove FIFO risposta**: evita accumulo di file temporanei
- **Lettura file a blocchi**: efficiente per file grandi
- **Thread pool fisso**: evita overhead creazione/distruzione thread
- **Cache con mutex per ogni entry**: massimizza parallelismo tra calcoli diversi

### Limitazioni note
- Cache illimitata: potrebbe crescere indefinitamente, non vi è un sistema per rimuovere quelle già inserite (potrebbe utilizzare un concetto di "data di scadenza")
- Nessuna autenticazione client
- FIFO può saturarsi con molti client simultanei

### Possibili estensioni per un utilizzo intensivo
- Implementare cache LRU con dimensione massima
- Supporto per hash multipli (MD5, SHA-512, ecc.)
- Statistiche server (hit rate, latenza media, throughput)
- Rate limiting per client

## Conclusioni

Il progetto dimostra l'implementazione di un servizio concorrente per il calcolo di hash SHA-256. L'architettura basata su **thread pool**, **coda prioritaria** e **cache condivisa** garantisce:

- **Efficienza**: riutilizzo di hash già calcolati e velocità di processazione grazie ai thread
- **Correttezza**: sincronizzazione e nessuna race condition
- **Scalabilità**: gestione concorrente di client multipli

## Github
Il codice sorgente è stato anche caricato su github, con github action che eseguono unit test ogni nuova versione per garantire la correttezza del codice.
[link alla repository](https://github.com/mp97dev/SHA256)
