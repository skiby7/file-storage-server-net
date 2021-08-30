---
author:
	- "Leonardo Scoppitto"
classoption: a4paper
documentclass: article
fontsize: 11pt
geometry: "left=2cm,right=2cm,top=2cm,bottom=2cm"
output:
	pdf_document:
		latex_engine: xelatex
title: Relazione file storage server
---

\hypersetup{linkcolor=black}

\pagebreak
\renewcommand{\contentsname}{Indice}
\tableofcontents
\pagebreak

# Introduzione

Il progetto è stato testato sulla macchina virtuale fornita dai docenti con 2 cores e **almeno** 2GB di ram.
È possibile consultare il repository di git al [seguente link](https://github.com/Skiby7/file-storage-server),
Sono state sviluppate delle parti opzionali quali:

* Sono stati realizzati più algoritmi di rimpiazzamento (vedere sezione numero)

* L'opzione -x per il client

* La compressione dei file presenti nel server


# Compilazione ed esecuzione

Una volta scompattato l'archivio, sarà sufficiente spostarsi dentro la root del progetto, compilare il progetto con il comando `make` o `make -j`, dopodiché i file eseguibili si troveranno dentro `bin`. Inoltre, è possibile testare il progetto con i comandi `make testX` dove `X` è il numero del test che si vuole eseguire. 
Nel caso si volesse clonare il progetto direttamente da Github, è necessario usare l'opzione `--recursive` per scaricare anche il sorgente di zlib, necessario per la compilazione del progetto:
```bash
git clone https://github.com/Skiby7/file-storage-server --recursive
```
Inoltre, appena scaricato il progetto da Github, non sono presenti i file necessari per eseguire i test, perciò la compilazione richiederà qualche minuto, in quanto verrà eseguito uno script che genererà dei file di testo. 

## Makefile targets

Oltre a quelli definiti nella specifica, è possibile usare anche i seguenti target:

* `clean`: rimuove le librerie statiche, gli eseguibili e eventuali altri file generati durante la compilazione

* `clean_files`: rimuove i file usati per i test

* `clean_all`: esegue `clean` e `clean_files`

* `gen_files`: genera i file testuali usati per i test

* `test3_un`, `test3_quiet`, `test3_un_quiet`: per eseguire il test 3 rispettivamente con la compressione disabilita, con il parametro `TUI` del file di configurazione disabilitato (*vedere sezione 3.1*) e, infine, con la compressione e `TUI` disabilitati. 

# Server

L'applicazione `server` consiste in un programma multithreaded che gestisce più richieste contemporanee di connessione da parte di client diversi. I client comunicano col server tramite un'API, la cui implementazione si trova in `fssApi.c`, e dopodiché vengono serviti da un worker del server il quale esegue un'operazione per poi rimettersi in attesa.

## File di configurazione

Il file di configurazione, passato come argomento da linea di comando all'avvio, è un generico file di testo contenente le seguenti parole chiave seguiti dal carattere `':'` : `WORKERS`, `MAXMEM`, `MAXFILES`, `SOCKNAME`, `LOGFILE`, `TUI`, `REPLACEMENT_ALGO`, `COMPRESSION` e `COMPRESSION_LEVEL`.
Si può definire `SOCKNAME` come path assoluto o, altrimenti, semplicemente come il nome che si vuole dare al socket, che verrà creato in `/tmp`.
Le voci `LOGFILE`, `REPLACEMENT_ALGO`, `COMPRESSION` e `TUI` sono opzionali:

* Se `LOGFILE` non è definito non verrà prodotto il file di log. Per specificare il file di log si può usare sia un path relativo che assoluto.

* `REPLACEMENT_ALGO` può essere impostato sui valori `FIFO`, `LRU`, `LFU`, `LRFU`. Se non specificato, viene usato l'algoritmo `FIFO`

* `TUI` (l'acronimo di *textual user interface*), indica se si vuole stampare sullo standard output un sommario della configurazione e visualizzare in tempo reale la quantità di file presenti nel server e la dimensione occupata. Se vale `y`, l'output verrà prodotto, altrimenti no.  

* Per attivare la compressione basta settare la voce `COMPRESSION` su `y`, specificando il livello di compressione settando `COMPRESSION_LEVEL` su un valore compreso fra 0-9 (dove 9 è il massimo della compressione) o, se non si specifica, verrà automaticamente impostato su 6.

Se manca una voce non opzionale o l'input di una voce non opzionale non è valido, il server non parte. Inoltre è possibile inserire linee vuote e definire commenti con il carattere `#` (tutto quello che segue `#` verrà ignorato). L'implementazione del parser è contenuta in `parser.c` e `parser.h`.


## Polling

In `server.c` è possibile consultare l'implementazione del thread Master e del signal handler. Una volta fatto partire il server, il thread main si mette in ascolto della `struct pollfd com_fd` (la quale viene creata con dimensione `DEFAULTFDS` e riallocata dinamicamente una volta che i client connessi aumentano) con una `poll()`. Le prime 3 posizioni di `com_fd` sono riservate come segue:

0. il socket su cui eseguire l'`accept()`

1. la pipe sulla quale i thread Worker restituiscono i file descriptor dei client ancora connessi (`good_fd_pipe`) 

2. la pipe su cui, sempre i thread Worker, restituiscono i file descriptor dei client che hanno effettuato la disconnessione, che sono crashati o che hanno con un fd corrotto (`done_fd_pipe`), così da chiuderli e aggiornare il numero di client connessi (`clients_active`).

## Signal handling e terminazione

La gestione dei segnali è demandata a un thread dedicato, in modo da non far interferire i segnali stessi con la `poll()` e le altre chiamate di sistema. All'avvio del server, infatti, vengono mascherati tutti i segnali ad eccezione di `SIGSEGV` e viene poi avviato il thread `signal_handler_thread` sulla routine `sig_wait_thread()`. Qui, il thread si mette in ascolto dei seguenti segnali:

* `SIGINT` e `SIGQUIT`: alla ricezione di uno di questi due segnali, il flag globale `abort_connections` viene settato su `true`, mentre `can_accept` viene settato su `false`. Dopodiché viene inviata la stringa *termina* al thread Master tramite `good_fd_pipe`, così che, nel caso fosse in attesa sulla `poll()`, si svegli e termini.

* `SIGHUP`: in questo caso il thread handler setta solo `can_accept` su `true`, in modo che il Master non accetti più connessioni e attenda che il counter dei client attivi arrivi a `0` per poi terminare.

* `SIGUSR1`: questo segnale è dedicato alla terminazione di `signal_handler_thread`, infatti gli viene inviato dal thread Master prima di terminare.

Il thread master, una volta uscito dal loop principale, svuota la coda dei lavori e la riempie con il valore `-2` per poi svegliare i thread Worker e farli terminare. Dopodiché cancella il thread `use_stat_thread`, invia il segnale `SIGUSR1` al `signal_handler_thread`, logga gli ultimi dati ed infine libera la memoria allocata dinamicamente.

## Storage

L'implementazione dello storage vero e proprio può essere consultata nei file `file.c` e `file.h`, dove sono definite le funzioni che i workers possono usare per modificare i dati all'interno del server. Ho deciso di implementare lo storage con una tabella hash con liste di trabocco, la cui dimensione è 1,33 volte il numero massimo di file che il server può gestire, così da mantenere il fattore di carico sotto il 75% e avere delle buone prestazioni (come da letteratura). Per il calcolo dell'hash ho usato l'algoritmo di Peter Jay Weinberger (la cui [implementazione](http://didawiki.cli.di.unipi.it/lib/exe/fetch.php/informatica/sol/laboratorio21/esercitazionib/icl_hash.tgz) è stata fornita a laboratorio). \
Le strutture che definiscono la memoria del server, `fss_file_t` e `fss_storage_t`, sono consultabili in `file.h`:

* `fss_file_t` contiene, oltre che i metadati e i dati del file stesso, due mutex, una *condition variable* e due contatori `readers` e `writers` con i quali ho implementato la procedura *lettore-scrittore* vista a lezione. La struttura contiene, inoltre, il timestamp dell'ultimo accesso, il timestamp della creazione e un contatore di utilizzo `use_stat`, che vedremo in seguito nella sezione dedicata all'algoritmo di rimpiazzamento.

* `fss_storage_t` contiene, oltre alla tabella hash, la dimensione della tabella stessa e i parametri di configurazione, una mutex per garantire l'accesso mutualmente esclusivo all'intera tabella e delle variabili in cui salvare i dati per generare le statistiche al termine dell'esecuzione.

## Rimpiazzamento dei file

Come detto prima, ogni file ha un contatore `use_stat` il quale viene impostato a 16 al momento della creazione e varia fra 0 e 32. Viene quindi aumentato ogni volta che il file viene letto, scritto (solo in questo caso di 2 punti) o bloccato e viene decrementato da un thread dedicato che viene svegliato ogniqualvolta viene effettuata una scrittura o viene creato un nuovo file.
Inoltre, ogni volta che si accede a un file, viene aggiornato il suo campo `last_access`.
Una volta che si deve liberare dello spazio in memoria, viene chiamata la funzione `select_victim()`, che non fa altro che scorrere tutta la tabella e copiare i metadati dei file (eccetto del file che ha causato la chiamata di `select_victim()`) in un array di tipo `victim_t`. Questo array viene poi ordinato con `qsort()` e, finché non viene liberata abbastanza memoria, si elimina il file con pathname uguale a `victims[i].pathname`.
Di seguito la funzione compare:

```c
static int compare(const void *a, const void *b) {
	victim_t a1 = *(victim_t *)a, b1 = *(victim_t *)b; 
	switch (server_storage.replacement_algo){
		case FIFO:
			return a1.created_time - b1.created_time;
		case LRU: 
			return a1.last_access - b1.last_access;
		case LFU:
			return a1.use_stat - b1.use_stat;
		case LRFU:
			if((a1.use_stat - b1.use_stat) != 0)
				return a1.use_stat - b1.use_stat;

			else return a1.last_access - b1.last_access;
	}
	return a1.created_time - b1.created_time;
}
```
Come si può vedere, nel caso di una politica `FIFO`, ordino gli elementi solo in base al tempo di creazione, con una politica `LRU` in base all'ultimo accesso effettuato, con una politica `LFU` in base al valore di `use_stat`, mentre con una politica `LRFU` si ordina prima in base a `use_stat` e, se questo è uguale per i due file, si confronta chi è stato usato più recentemente.
Il thread `use_stat_thread`, che esegue la routine `use_stat_update()`, inoltre, si occupa di sbloccare eventuali file rimasti inutilizzati con `use_stat` uguale 0, passando la lock ai client in attesa. 
<!-- 
Ogni volta che avviene una scrittura (sia `WRITE`, che `APPEND`) viene controllato che lo storage non abbia raggiunto il numero massimo di file che può contenere né che il file sia troppo grande né che non ci sia abbastanza spazio libero. Se la condizione non è soddisfatta, si va alla ricerca della vittima: viene allocato un array di `victim_t`, ovvero una struttura in cui verranno copiate le statistiche di utilizzo `use_stat`, il timestamp dell'ultimo accesso al file, la dimensione e il nome di ogni file presente nello storage. Dopo aver inizializzato l'array con i metadati di tutti i file, questo, viene ordinato con `qsort()` in base al campo `use_stat` e, nel caso questo fosse uguale per due file, in ordine crescente rispetto all'ultimo accesso. Questo approccio permette di rimuovere dallo storage i file meno usati e fra questi, a parità di utilizzo, scegliere in base all'ultimo accesso.\

Il contatore `use_stat` viene incrementato di 2 unità ogni volta che il file viene scritto, mentre di 1 punto quando viene letto con una `READ` (non `READ_N`) o se viene bloccato con una `LOCK`. Viene invece decrementato da un thread dedicato `use_stat_update` che, ogniqualvolta viene eseguita un'operazione di scrittura o di una open-create, visita tutto lo storage diminuendo di una unità il contatore. Il contatore di ogni file viene inizializzato a 16, ha un valore massimo di 32 e, una volta che ha raggiunto lo 0, se sul file era stata eseguita la lock, questo, se non viene utilizzato da più di 2 minuti, viene sbloccato automaticamente e la lock viene passata ad un eventuale client in attesa. -->

# Client

Dal momento che i *file descriptor* lato server usati per la comunicazione con il client non sono univoci, ma vengono riciclati, ogni client viene identificato dal server tramite il suo PID, che invece, avendo come limite superiore 2^22^, si può considerare univoco. Un possibile sviluppo del progetto potrebbe essere introdurre un flag (ad esempio `-n`) col quale specificare un *token* o un nome univoco con cui autenticarsi in modo da poter lavorare sugli stessi file, magari lasciandoli anche bloccati, in sessioni diverse.

## Utilizzo dei path

Come da specifica, ogni file è identificato dal suo path assoluto (sul quale viene anche calcolato l'hash), pertanto, per evitare ambiguità, tutte le operazioni del client che prendono in input un file richiedono un path assoluto, altrimenti l'operazione fallirà. Fanno eccezione le opzioni `-d`, `-D` e `-w`, poiché prendono in input una cartella e se ne occuperà il client a trasformare il path da relativo ad assoluto. 

## Salvataggio dei file sul disco

I file che il server invia al client, vengono salvati nella cartella specificata con i flag `-d` o `-D` ricostruendo l'albero delle directory.

# Comunicazione client-server

La comunicazione fra il client (C) e il server (S) avviene tramite il protocollo richiesta-risposta come segue:

1. C: Invia la lunghezza della richiesta

2. S: Il thread Master invia il file descriptor del client al thread Worker che legge la lunghezza della richiesta, invia un byte di *acknowledge* al client e alloca la memoria per leggere la richiesta

3. C: Riceve l'acknowledge, invia il pacchetto con la richiesta serializzata e si mette in attesa

4. S: Riceve la richiesta, la processa, serializza la risposta e invia la dimensione della risposta

5. C: Legge la lunghezza della risposta invia un byte di acknowledge e alloca la memoria per leggere la risposta, dopodiché la processa

6. S: Il thread Worker invia il descrittore del socket del client al thread Master e si rimette in attesa

Tutti i dati, sia la lunghezza del pacchetto, che il pacchetto, vengono inviati come stringhe di byte (`unsigned char *`). L'implementazione della conversione da `uint32_t` e `uint64_t` ad array di byte e la serializzazione/deserializzazione delle richieste possono essere consultate in `serialization.c` e `serialization.h`


## Comandi

Ho deciso di condensare sia i comandi che i codici di errore in un byte per semplicità. Al momento, avendo 8 operazioni possibili ho usato un bit per ogni operazioni, ma, nel caso si volessero implementare delle nuove operazioni, si potrebbe usare una combinazione di bit per identificarla, dal momento che a 1 richiesta corrisponde 1 operazione.

| Operazione 	| Valore 	| Descrizione                                                         	|
|------------	|:--------:	|---------------------------------------------------------------------	|
| OPEN       	| 0x01   	| Operazione di apertura file                                         	|
| CLOSE      	| 0x02   	| Operazione di chiusura file                                         	|
| READ       	| 0x04   	| Operazione di lettura file                                          	|
| WRITE      	| 0x08   	| Operazione di scrittura nuovo file                                  	|
| APPEND     	| 0x10   	| Operazione di scrittura in append a un file                         	|
| REMOVE     	| 0x20   	| Operazione di rimozione file                                        	|
| QUIT       	| 0x40   	| UNUSED                                                              	|
| SET_LOCK   	| 0x80   	| Operazione di lock/unlock file:                                     	|
|            	|        	| se il campo  flag  della richiesta vale  `O_LOCK`,                  	|
|            	|        	| viene eseguita l'operazione di lock,  altrimenti si esegue l'unlock 	|

I flag `O_CREATE` e `O_LOCK` hanno rispettivamente il valore `0x01` e `0x02`

## Errori

Ho deciso di definire dei codici di errore per estendere gli errori riportati in `errno.h` e dare una spiegazione più specifica del problema, come ad esempio una mancata lock, una open ripetuta o la creazione di un file già esistente. Gli errori specifici vengono inviati al client nel campo `code[2]` della risposta e si trovano alla posizione 0 (vedere sezione successiva). Di seguito la tabella: 

| Codice                 	| Valore 	| Descrizione                                        	|
|------------------------	|:--------:	|----------------------------------------------------	|
| FILE_OPERATION_SUCCESS 	|  `0x01` 	| L'operazione è stata completata con successo       	|
| FILE_OPERATION_FAILED  	|  `0x02`  	| Si è verificato un errore e                        	|
|                        	|        	| l'operazione non è stata completata                	|
| FILE_ALREADY_OPEN      	|  `0x04`  	| Il file che si sta cercando di aprire è già aperto 	|
| FILE_ALREADY_LOCKED    	|  `0x08`  	| Il file che si sta cercando di bloccare è          	|
|                        	|        	| già bloccato dal client chiamante                  	|
| FILE_LOCKED_BY_OTHERS  	|  `0x10`  	| Il file che si sta cercando di bloccare            	|
|                        	|        	| o modificare è bloccato da un altro client         	|
| FILE_NOT_LOCKED        	|  `0x20`  	| Il file non è stato bloccato prima di              	|
|                        	|        	| un'operazione che richiede la lock                 	|
| FILE_NOT_OPEN        		|  `0x40`  	| Il file non è stato aperto 							|
| STOP        				|  `0x80`  	| Segnale di stop per la `READ_N`						|


## `client_request` e `server_response`

Sia il server che il client utilizzano le seguenti strutture per organizzare le richieste e le risposte da inviare:

```c
typedef struct client_request_{
	unsigned int client_id;	
	unsigned char command;	
	unsigned char flags;	
	int files_to_read; 		
	unsigned int pathlen;	
	char *pathname;			
	unsigned long size;		
	unsigned char* data;
} client_request;

typedef struct server_response_{
	unsigned int pathlen;
	char *pathname;
	unsigned char has_victim;
	unsigned char code[2];
	unsigned long size;
	unsigned char* data;
} server_response;
```

I campi in comune per la richiesta e la risposta sono: `pathlen` che indica la lunghezza della stringa `pathname` (compreso `\0`) e `size` il quale indica la lunghezza del campo `data`, che a sua volta contiene i dati (non compressi) del file inviato/ricevuto. Per quanto riguarda `client_request`, partendo dall'alto si trova:

* `client_id`: `PID` del processo che invia la richiesta

* `command`: comando da eseguire (vedi *5.1 Comandi*)

* `flags`: assume i valori `O_LOCK` e/o `O_CREATE`

* `files_to_read`: numero di file da leggere per l'operazione *readNFile* 

Mentre, per `server_response` abbiamo:

* `has_victim`: indica se dopo la prima risposta il client si deve rimettere in attesa per ricevere i file eliminati in seguito a una scrittura. Vale 0 o 1

* `code[2]`: all'indice 0 si ha l'esito dell'operazione (vedi sezione precedente), mentre all'indice 1 si trova un eventuale valore di errore fra quelli definiti in `errno.h` e `errno-base.h`

## Serializzazione e deserializzazione

Ho deciso di serializzare le richieste, le risposte e gli interi, piuttosto che inviare campo per campo in modo da utilizzare un numero minore di read (una per la dimensione del pacchetto e una per il pacchetto stesso) e in modo tale da rendere l'applicazione compatibile con la comunicazione via rete senza apportare troppe modifiche, a questo scopo ho utilizzato, per i dati che vengono inviati tramite il socket i tipi definiti in `stdint.h`.
In `serialization.c` e `serialization.h` si trova l'implementazione delle funzioni usate per la serializzazione.

# Parti aggiuntive

## Compressione

Ho implementato la compressione dei file usando la libreria open-source `zlib`. Nel file di configurazione questa opzione può essere abilitata impostando il parametro `COMPRESSION` su `y` e si può impostare il livello di compressione su un valore compreso fra 1, che è il minimo e 9, che è il massimo (il livello 0 è usato da `zlib` per creare un archivio di più file senza però comprimerli).
La libreria è stata compilata con il flag `--const` e come libreria statica.

### Copyright notice

Direttamente dal README del [repository](https://github.com/madler/zlib) ufficiale:
```
 (C) 1995-2017 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

## Interfaccia testuale

Per rendere più interattivo il test3 e per avere qualche informazione utile durante l'uso del server, ho implementato una semplice interfaccia testuale con la quale avere una panoramica sia della configurazione del server, che dell'uso delle risorse del server. Per ridurre al minimo l'impatto sulle performance, invece che far ridisegnare a ogni thread Worker l'intera schermata ogni volta che esegue un'operazione che modifichi i file in numero o in dimensione, ho preferito utilizzare un thread dedicato, il quale si mette in ascolto con una `poll()` su una pipe. La pipe viene utilizzata dai Worker per notificare l'inizio e la fine di un'operazione di `READ`/`READ END` o di `WRITE`/`WRITE END`, così che il thread possa ridisegnare l'interfaccia in base all'azione eseguita. L'accesso alle informazioni dello storage, quindi la lock della mutex globale dello storage, viene effettuato solo quando viene ricevuta una `WRITE END`. 

## Opzione -x

Ho introdotto, per il client, l'opzione `-x` da usare dopo `-w` per permettere di sbloccare tutti i file caricati, questo per avere la possibilità di caricare un'intera cartella, senza dover sbloccare a mano ogni file.
