# Relazione: Servizio Client-Server SHA-256

## 1. Introduzione e Requisiti

### 1.1 Obiettivo del Progetto
Il progetto implementa un sistema client-server per il calcolo di hash crittografici SHA-256 di file. Il sistema è progettato per fornire un servizio efficiente e scalabile per l'hashing di file, con funzionalità di caching e gestione delle priorità.

### 1.2 Requisiti Funzionali
- **Calcolo SHA-256**: Implementazione del calcolo dell'hash SHA-256 per file di qualsiasi dimensione
- **Architettura Client-Server**: Comunicazione tra client e server tramite named pipes (FIFO)
- **Sistema di Caching**: Memorizzazione dei digest calcolati per evitare ricalcoli
- **Gestione delle Priorità**: Ordinamento delle richieste in base alla dimensione del file
- **Supporto Multi-threading**: Pool di thread per gestire richieste concorrenti
- **Query della Cache**: Possibilità di interrogare lo stato della cache del server

### 1.3 Requisiti Non Funzionali
- **Efficienza**: Utilizzo di buffer di dimensione ottimale (4096 byte) per la lettura dei file
- **Scalabilità**: Pool di thread configurabile (default: 4 thread)
- **Thread-safety**: Sincronizzazione thread-safe tramite mutex e variabili condizione
- **Robustezza**: Gestione degli errori e dei casi limite (file vuoti, file inesistenti)

## 2. Architettura e Implementazione

### 2.1 Architettura Generale
Il sistema è composto da due componenti principali:

```
┌─────────────────────────────────────────────────────────┐
│                        CLIENT                           │
│  - Crea FIFO di risposta privata                        │
│  - Invia richiesta al server (via FIFO globale)         │
│  - Attende risposta sulla propria FIFO                  │
└───────────────────────┬─────────────────────────────────┘
                        │
                        │ FIFO Request (/tmp/sha256_req_fifo)
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│                       SERVER                            │
│  ┌───────────────────────────────────────────┐          │
│  │         Main Thread (Listener)            │          │
│  │  - Riceve richieste sulla FIFO globale    │          │
│  │  - Calcola dimensione file                │          │
│  │  - Inserisce nella Priority Queue         │          │
│  └────────────┬──────────────────────────────┘          │
│               │                                          │
│               ▼                                          │
│  ┌───────────────────────────────────────────┐          │
│  │         Priority Queue                    │          │
│  │  - Ordina richieste per dimensione file   │          │
│  │  - File più piccoli hanno priorità        │          │
│  └────────────┬──────────────────────────────┘          │
│               │                                          │
│               ▼                                          │
│  ┌───────────────────────────────────────────┐          │
│  │       Thread Pool (4 workers)             │          │
│  │  - Estrae richieste dalla coda            │          │
│  │  - Controlla la cache                     │          │
│  │  - Calcola SHA-256 se necessario          │          │
│  │  - Aggiorna cache                         │          │
│  │  - Invia risposta al client               │          │
│  └────────────┬──────────────────────────────┘          │
│               │                                          │
│               ▼                                          │
│  ┌───────────────────────────────────────────┐          │
│  │              Cache                        │          │
│  │  - Lista concatenata di entry             │          │
│  │  - Ogni entry: filepath + digest          │          │
│  │  - Thread-safe con mutex per entry        │          │
│  └───────────────────────────────────────────┘          │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Componenti Principali

#### 2.2.1 Client (`client.c`)
Il client è responsabile di:
1. Creare una FIFO di risposta personale (`/tmp/sha256_resp_<PID>_fifo`)
2. Preparare e inviare la richiesta al server tramite FIFO globale
3. Attendere la risposta dal server sulla propria FIFO
4. Gestire il comando speciale `CACHE?` per interrogare la cache

**Funzionalità speciali**:
- Supporto per il comando `CACHE?` che permette di visualizzare lo stato della cache senza attendere risposta
- Distinzione tra cache hit e cache miss nella risposta

#### 2.2.2 Server (`server.c`)
Il server implementa:
- **Main Thread**: Ascolta sulla FIFO globale, riceve richieste e le inserisce nella priority queue
- **Thread Pool**: 4 worker thread che processano le richieste in parallelo
- **Priority Scheduling**: Le richieste vengono ordinate in base alla dimensione del file (file più piccoli hanno priorità)

**Flusso di elaborazione**:
```
1. Ricezione richiesta → 2. Calcolo dimensione → 3. Inserimento in coda
                                                           ↓
4. Worker estrae richiesta ← 5. Ordine per dimensione ←──┘
         ↓
5. Ricerca in cache
         ├─→ [CACHE HIT] → Risposta immediata
         └─→ [CACHE MISS] → Calcolo SHA-256 → Aggiornamento cache → Risposta
```

#### 2.2.3 Priority Queue (`queue.c`, `queue.h`)
Implementazione di una coda con priorità basata su lista concatenata:
- **Ordinamento**: Inserimento ordinato per dimensione file (crescente)
- **Thread-safety**: Mutex per accesso concorrente
- **Blocking**: `queue_pop()` è bloccante quando la coda è vuota (condition variable)
- **Algoritmo**: Scansione lineare per trovare la posizione di inserimento corretta

**Vantaggi**: I file più piccoli vengono processati prima, migliorando il throughput e riducendo i tempi di attesa medi.

#### 2.2.4 Cache (`cache.c`, `cache.h`)
Sistema di caching intelligente con sincronizzazione avanzata:

**Struttura**:
- Lista concatenata di entry
- Ogni entry contiene: filepath, digest, flag `ready`, mutex e condition variable

**Meccanismo di sincronizzazione**:
1. **Lookup**: Se l'entry esiste ed è pronta → cache hit immediato
2. **Waiting**: Se l'entry esiste ma non è pronta → il thread aspetta sulla condition variable
3. **Computation**: Se l'entry non esiste → viene creata con `ready=0`, si calcola il digest, si aggiorna con `ready=1` e si notificano tutti i thread in attesa

**Vantaggi**:
- Evita calcoli ridondanti: se più client richiedono lo stesso file contemporaneamente, solo un thread calcola l'hash
- I thread successivi aspettano il risultato invece di ricalcolarlo
- Thread-safe: ogni entry ha il proprio mutex

#### 2.2.5 SHA-256 Utility (`sha256_utils.c`, `sha256_utils.h`)
Implementazione del calcolo dell'hash SHA-256:
- Utilizza la libreria OpenSSL
- Lettura del file a blocchi (4096 byte)
- Conversione del digest binario in stringa esadecimale
- Gestione degli errori (file non trovato, errori di lettura)

### 2.3 Comunicazione IPC
Il sistema utilizza **Named Pipes (FIFO)** per la comunicazione:

**FIFO Globale** (`/tmp/sha256_req_fifo`):
- Creata dal server all'avvio
- Utilizzata da tutti i client per inviare richieste
- Modalità: O_RDONLY | O_NONBLOCK (server), O_WRONLY (client)

**FIFO di Risposta** (`/tmp/sha256_resp_<PID>_fifo`):
- Creata dal client prima di inviare la richiesta
- Univoca per ogni client (basata sul PID)
- Utilizzata dal server per inviare la risposta
- Eliminata dal client dopo aver ricevuto la risposta

**Strutture di comunicazione**:
```c
// Client → Server
typedef struct {
    pid_t pid;                        // PID del client
    char filepath[PATH_MAX_LEN];      // Path del file
} sha256_request_t;

// Server → Client
typedef struct {
    char digest[DIGEST_LEN];          // Digest SHA-256 (65 byte)
    int status;                       // 0=ok, 1=error, 2=cache miss, 3=cache hit
} sha256_response_t;
```

### 2.4 Tecnologie e Librerie

**Linguaggio**: C (standard C11)

**Librerie utilizzate**:
- **OpenSSL**: Per il calcolo SHA-256 (`libssl-dev`)
- **POSIX Threads (pthread)**: Per il multi-threading
- **System Calls POSIX**: Per FIFO, file I/O, sincronizzazione

**Build System**: CMake 3.10+

**Dipendenze**:
```cmake
- OpenSSL (REQUIRED)
- pthread (linked con -lpthread)
```

## 3. Risultati dei Test

### 3.1 Setup di Test
Il sistema include uno script di test automatizzato (`run_test.sh`) che:
1. Compila il progetto con CMake
2. Avvia il server in background
3. Esegue il client con un file di test
4. Verifica che il digest calcolato corrisponda a quello generato da tool CLI standard
5. Interroga la cache
6. Termina il server correttamente

### 3.2 Test Case Eseguiti

#### Test 1: File Normale (`test/testfile.txt`)
```
Contenuto: "123"
Dimensione: 3 byte
SHA-256 atteso: a665a45920422f9d417e4867efdc4fb8a04a1f3fff1fa07e998e86f7f7a27ae3
Risultato: ✅ PASS - Digest corretto
Tempo: ~1 secondo (incluso build e startup)
```

#### Test 2: File con Password (`test/password.txt`)
```
Contenuto: "VeryToughPassword123!@🚀"
Dimensione: 26 byte (22 byte ASCII + 4 byte UTF-8 per emoji 🚀)
SHA-256 atteso: 7eb03520f980bcd651a8d4a876a86afd4a5e98488e2b8b298dd984b625e7999e
Risultato: ✅ PASS - Digest corretto, gestione corretta di caratteri UTF-8
Tempo: ~1 secondo
```

#### Test 3: File Vuoto (`test/emptyfile.txt`)
```
Contenuto: (vuoto)
Dimensione: 0 byte
SHA-256 atteso: e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855
Risultato: ✅ PASS - Gestione corretta del caso limite (file vuoto)
Tempo: ~1 secondo
```

#### Test 4: Query Cache
```
Comando: ./build/client CACHE?
Risultato: ✅ PASS - Il server stampa correttamente lo stato della cache
Output: Mostra tutte le entry presenti con filepath, digest e stato (READY/PENDING)
```

### 3.3 Validazione
Ogni test esegue automaticamente:
1. **Confronto con CLI**: Il digest calcolato viene confrontato con quello prodotto da `sha256sum`, `shasum -a 256`, o `openssl dgst -sha256`
2. **Verifica formato**: Il digest deve essere una stringa esadecimale di 64 caratteri
3. **Verifica comunicazione**: Client e server devono comunicare correttamente tramite FIFO

### 3.4 Copertura dei Test
- ✅ File normali con contenuto testuale
- ✅ File vuoti (0 byte)
- ✅ Caratteri UTF-8 e caratteri speciali
- ✅ Query della cache
- ✅ Comunicazione IPC
- ✅ Sincronizzazione multi-thread (implicita)

### 3.5 Performance
**Osservazioni**:
- Il calcolo dell'hash è molto veloce per file piccoli (<1ms)
- Il tempo totale di test (~1 secondo) è dominato da build e startup del server
- La cache riduce drasticamente il tempo per richieste duplicate (da calcolo completo a lookup immediato)
- Il sistema di priorità favorisce l'elaborazione rapida di file piccoli

## 4. Punti di Forza

1. **Architettura modulare**: Separazione chiara delle responsabilità (queue, cache, client, server)
2. **Sincronizzazione robusta**: Uso corretto di mutex e condition variables
3. **Efficienza**: Caching intelligente che evita calcoli ridondanti
4. **Priority scheduling**: File piccoli hanno priorità, migliorando il throughput
5. **Scalabilità**: Pool di thread configurabile
6. **Gestione errori**: Controllo degli errori su operazioni di I/O e allocazione memoria
7. **Portabilità**: Uso di API POSIX standard

## 5. Possibili Miglioramenti

### 5.1 Miglioramenti Funzionali

#### 5.1.1 Limite Dimensione Cache
**Problema attuale**: La cache cresce indefinitamente, potenzialmente occupando molta memoria.

**Soluzione proposta**:
- Implementare una politica LRU (Least Recently Used) per limitare il numero di entry
- Aggiungere un timestamp di ultimo accesso a ogni entry
- Definire un limite massimo di entry (es. 1000)
- Quando si supera il limite, rimuovere l'entry meno recentemente utilizzata

```c
typedef struct cache_entry {
    // ... campi esistenti ...
    time_t last_access;               // Timestamp ultimo accesso
    struct cache_entry *lru_prev;     // Pointer per lista LRU
    struct cache_entry *lru_next;     // Pointer per lista LRU
} cache_entry_t;
```

#### 5.1.2 Persistenza della Cache
**Problema attuale**: La cache viene persa quando il server termina.

**Soluzione proposta**:
- Serializzare la cache su file al momento dello shutdown
- Ricaricare la cache all'avvio del server
- Formato suggerito: JSON o binario con checksum

#### 5.1.3 Invalidazione Cache
**Problema attuale**: Se un file viene modificato, la cache contiene un digest obsoleto.

**Soluzione proposta**:
- Memorizzare anche il timestamp di modifica del file (`st_mtime`)
- Prima di restituire un risultato dalla cache, verificare se il file è stato modificato
- Se modificato, ricalcolare l'hash e aggiornare la cache

```c
typedef struct cache_entry {
    // ... campi esistenti ...
    time_t file_mtime;                // Timestamp modifica file
} cache_entry_t;
```

#### 5.1.4 Supporto per File di Grandi Dimensioni
**Problema attuale**: File molto grandi potrebbero rallentare l'intero sistema.

**Soluzione proposta**:
- Implementare un timeout configurabile per il calcolo dell'hash
- Permettere al client di specificare una priorità personalizzata
- Considerare l'uso di algoritmi di hashing più veloci per file molto grandi (es. xxHash per pre-screening)

### 5.2 Miglioramenti di Sicurezza

#### 5.2.1 Aggiornamento API OpenSSL
**Problema attuale**: Il codice usa le API legacy (`SHA256_Init`, `SHA256_Update`, `SHA256_Final`) che sono state deprecate in favore dell'interfaccia EVP in OpenSSL 3.0.

**Soluzione proposta**:
- Migrare alla nuova EVP API di OpenSSL 3.0:
```c
EVP_MD_CTX *ctx = EVP_MD_CTX_new();
EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
EVP_DigestUpdate(ctx, buffer, bytes_read);
EVP_DigestFinal_ex(ctx, hash, &hash_len);
EVP_MD_CTX_free(ctx);
```

#### 5.2.2 Validazione Input
**Problema attuale**: Path dei file non completamente validati.

**Soluzione proposta**:
- Verificare che i path non contengano sequenze pericolose (`../`, path assoluti inaspettati)
- Limitare l'accesso a directory specifiche (sandboxing)
- Implementare una whitelist di directory accessibili

#### 5.2.3 Gestione Risorse
**Problema attuale**: In caso di errore, alcune risorse potrebbero non essere rilasciate.

**Soluzione proposta**:
- Implementare cleanup handlers più robusti
- Usare `pthread_cleanup_push`/`pthread_cleanup_pop` per garantire il cleanup
- Aggiungere signal handler per SIGINT, SIGTERM per shutdown pulito

### 5.3 Miglioramenti di Usabilità

#### 5.3.1 File di Configurazione
**Soluzione proposta**:
- Permettere configurazione tramite file (es. `config.ini`)
- Parametri configurabili:
  - Dimensione pool di thread
  - Dimensione massima cache
  - Path delle FIFO
  - Livello di logging
  - Timeout operazioni

#### 5.3.2 Logging Strutturato
**Problema attuale**: Output di log minimale e su stdout/stderr.

**Soluzione proposta**:
- Implementare sistema di logging con livelli (DEBUG, INFO, WARN, ERROR)
- Supporto per log su file rotazionali
- Timestamp per ogni messaggio
- Possibilità di configurare il livello di verbosità

#### 5.3.3 Interfaccia Client Migliorata
**Soluzione proposta**:
- Opzioni command-line più ricche:
  - `--timeout`: Timeout per la risposta
  - `--verbose`: Output dettagliato
  - `--json`: Output in formato JSON
  - `--batch`: Processare una lista di file

### 5.4 Miglioramenti di Performance

#### 5.4.1 Pool di Thread Dinamico
**Problema attuale**: Numero di thread fisso.

**Soluzione proposta**:
- Implementare auto-scaling del pool:
  - Aumentare il numero di thread quando la coda cresce
  - Ridurre il numero di thread quando il carico diminuisce
  - Definire min/max thread configurabili

#### 5.4.2 Prefetching Intelligente
**Soluzione proposta**:
- Analizzare i pattern di accesso ai file
- Pre-calcolare hash di file frequentemente richiesti
- Implementare meccanismo di "warm-up" della cache

#### 5.4.3 Ottimizzazione I/O
**Soluzione proposta**:
- Usare `mmap()` invece di `read()` per file di medie dimensioni
- Implementare I/O asincrono con `io_uring` (Linux moderno)
- Utilizzare `posix_fadvise()` per hint al kernel

### 5.5 Miglioramenti di Monitoring

#### 5.5.1 Statistiche Runtime
**Soluzione proposta**:
- Implementare endpoint per statistiche:
  - Numero di richieste processate
  - Cache hit rate
  - Tempo medio di elaborazione
  - Numero di thread attivi
  - Dimensione della coda

#### 5.5.2 Health Check
**Soluzione proposta**:
- Endpoint per verificare lo stato del server
- Possibilità di verificare se il server è responsive
- Supporto per monitoring tools esterni

### 5.6 Testing e Quality Assurance

#### 5.6.1 Test Suite Estesa
**Soluzione proposta**:
- Test unitari per ogni modulo
- Test di integrazione
- Test di stress e load testing
- Test di concorrenza (race conditions, deadlock)
- Fuzzing per robustezza

#### 5.6.2 Continuous Integration
**Soluzione proposta**:
- Configurare CI/CD pipeline (GitHub Actions, GitLab CI)
- Build automatici su multiple piattaforme
- Run automatici dei test
- Code coverage analysis
- Static analysis (cppcheck, clang-tidy)

## 6. Conclusioni

Il progetto implementa con successo un servizio client-server per il calcolo di hash SHA-256, dimostrando competenze in:
- Programmazione di sistemi POSIX
- Multi-threading e sincronizzazione
- Comunicazione inter-processo (IPC)
- Gestione della memoria e delle risorse
- Design di sistemi concorrenti

L'architettura è solida e ben strutturata, con una chiara separazione delle responsabilità. Il sistema di caching e il priority scheduling mostrano attenzione all'efficienza e alla performance.

I test eseguiti confermano la correttezza dell'implementazione per vari casi d'uso, inclusi casi limite come file vuoti e caratteri speciali.

I miglioramenti proposti potrebbero rendere il sistema ancora più robusto, sicuro e adatto a scenari di produzione, mantenendo al contempo la semplicità e l'eleganza dell'architettura attuale.

---

**Autore**: Sistema SHA-256 Client-Server  
**Data**: Gennaio 2026  
**Versione**: 1.0
