#include "work.h"
extern client_conf config;
extern unsigned char errno_summary;
void print_errno_summary(){
	switch (errno_summary){
		case FILE_ALREADY_OPEN:
			fprintf(stderr, ANSI_COLOR_RED"└"ANSI_COLOR_YELLOW"Il file era già aperto!"ANSI_COLOR_RESET_N);
			break;
		case FILE_ALREADY_LOCKED:
			fprintf(stderr, ANSI_COLOR_RED"└"ANSI_COLOR_YELLOW"Il file è già stato bloccato!"ANSI_COLOR_RESET_N);
			break;
		case FILE_LOCKED_BY_OTHERS:
			fprintf(stderr, ANSI_COLOR_RED"└"ANSI_COLOR_YELLOW"Il file è bloccato da un altro client!"ANSI_COLOR_RESET_N);
			break;
		case FILE_NOT_LOCKED:
			fprintf(stderr, ANSI_COLOR_RED"└"ANSI_COLOR_YELLOW"Il file non è stato bloccato prima di eseguire l'operazione!"ANSI_COLOR_RESET_N);
			break;
		case FILE_NOT_OPEN:
			fprintf(stderr, ANSI_COLOR_RED"└"ANSI_COLOR_YELLOW"Il file non è stato aperto prima di eseguire l'operazione!"ANSI_COLOR_RESET_N);
			break;
		
		default:
			break;
	}
	errno_summary = 0;
}

void check_errno(char* pathname, int retval, unsigned char op, int flag){
	if (retval >= 0) return;
	switch (op){
		case OPEN: 
			fprintf(stderr, ANSI_COLOR_RED"Errore openFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case CLOSE: 
			fprintf(stderr, ANSI_COLOR_RED"Errore closeFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case READ: 
			fprintf(stderr, ANSI_COLOR_RED"Errore readFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case READ_N: 
			fprintf(stderr, ANSI_COLOR_RED"Errore readNFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case WRITE: 
			fprintf(stderr, ANSI_COLOR_RED"Errore writeFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case APPEND: 
			fprintf(stderr, ANSI_COLOR_RED"Errore appendToFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case REMOVE: 
			fprintf(stderr, ANSI_COLOR_RED"Errore removeFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
			break;
		case SET_LOCK: 
			switch (flag){
				case 0:
					fprintf(stderr, ANSI_COLOR_RED"Errore unlockFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
					break;
				case O_LOCK:
					fprintf(stderr, ANSI_COLOR_RED"Errore lockFile %s -> %s"ANSI_COLOR_RESET_N, pathname, strerror(errno));
					break;
				
				default:
					break;
			}
			break;
		default:
			break;
		}
	print_errno_summary();
}

int handle_read_files(char *args, char *dirname){
	char *tmpstr = NULL;
	char *token = NULL;
	unsigned char *buffer = NULL;
	size_t buff_size = 0;
	int retval = 0;
	char current_dir[PATH_MAX];
	CHECKEXIT(getcwd(current_dir, sizeof current_dir) == NULL, true, "Errore getcwd");
	if(!dirname) if (config.verbose) fprintf(stderr, ANSI_COLOR_RED"Non è stata specificata la cartella di destinazione, i file non verranno salvati!"ANSI_COLOR_RESET_N);
	
	
	token = strtok_r(args, DELIM, &tmpstr);
	while(token){
		retval = openFile(token, 0);
		if(config.verbose) check_errno(token, retval, OPEN, 0);
		if(retval>= 0){
			retval = readFile(token, (void **)&buffer, &buff_size);
			if(config.verbose) check_errno(token, retval, READ, 0);
			if(config.verbose && retval >= 0) printf("Letti %lu bytes da %s\n", buff_size, token);
			retval = closeFile(token);
			if(config.verbose) check_errno(token, retval, CLOSE, 0);
		}
		if(dirname){
			if(config.verbose) CHECKERRNO(chdir(dirname) < 0, "Errore chdir");
			save_to_file(token, buffer, buff_size);
			if(config.verbose) CHECKERRNO(chdir(current_dir) < 0, "Errore chdir");
		}
		free(buffer);
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	return 0;
}

int handle_simple_request(char *args, const unsigned char command, const char* dirname){
	char *tmpstr = NULL;
	char *token = NULL;
	char *real_path = NULL;
	int open_file_val = 0, file = 0, retval = 0;
	size_t size = 0;
	struct stat st;
	unsigned char *buffer = NULL;
	token = strtok_r(args, DELIM, &tmpstr);
	
	while(token){
		errno = 0;
		if(command & WRITE_FILES){  
			if((file = open(token, O_RDONLY)) == -1){
				if (config.verbose) fprintf(stderr, "Errore durante l'apertura del file in locale %s -> %s\n", token, strerror(errno));
				token = strtok_r(NULL, DELIM, &tmpstr);
				continue;
			}
			close(file);
			open_file_val = openFile(token, O_CREATE | O_LOCK);
			if(open_file_val >= 0){
				retval = writeFile(token, dirname);
				if(config.verbose) check_errno(token, retval, WRITE, 0);
				retval = closeFile(token);
				if(config.verbose) check_errno(token, retval, CLOSE, 0);
				stat(token, &st);
				if(config.verbose) printf("Scritti %lu bytes (file: %s)\n", st.st_size, token);
			}
			else if(open_file_val <= 0 && errno == EEXIST){
				errno = 0;
				if(openFile(token, 0) < 0){
					if(config.verbose){
						fprintf(stderr, ANSI_COLOR_RED"Errore openFile -> %s"ANSI_COLOR_RESET_N, strerror(errno));
						print_errno_summary();
					} 
					// free(real_path);
					// real_path = NULL;
					token = strtok_r(NULL, DELIM, &tmpstr);
					continue;
				}
				if((file = open(token, O_RDONLY)) == -1){
					if(config.verbose) fprintf(stderr, "Errore durante l'apertura del file in locale %s -> %s\n", token, strerror(errno));
					// free(real_path);
					// real_path = NULL;
					token = strtok_r(NULL, DELIM, &tmpstr);
					continue;
				}
				if(fstat(file, &st) < 0){
					if (config.verbose) perror("Errore fstat");
					// free(real_path);
					// real_path = NULL;	
					token = strtok_r(NULL, DELIM, &tmpstr);
					continue;
				}
				size = st.st_size;
				buffer =  (unsigned char *) calloc(size, sizeof(unsigned char));
				if(read(file, buffer, size) < 0){
					if (config.verbose) fprintf(stderr, "Errore durante la lettura del file in locale %s -> %s\n", token, strerror(errno));
					token = strtok_r(NULL, DELIM, &tmpstr);
					continue;
				}
				retval = appendToFile(token, buffer, size, dirname);
				if(config.verbose) check_errno(token, retval, APPEND, 0);
				if(config.verbose && retval >= 0) printf("Appended %lu bytes (file: %s)\n", st.st_size, token);
				retval = closeFile(token);
				if(config.verbose) check_errno(token, retval, CLOSE, 0);
				free(buffer);
				buffer = NULL;
			}
			else{
				if (config.verbose) check_errno(token, open_file_val, OPEN, 0);
				// free(real_path);
				// real_path = NULL;
				token = strtok_r(NULL, DELIM, &tmpstr);
				continue;
			}
		}
		else if(command & LOCK_FILES){ 
			retval = lockFile(token);
			if(config.verbose) check_errno(token, retval, SET_LOCK, O_LOCK);
			if(config.verbose && retval >= 0) printf("Locked %s\n", token);

		}
		else if(command & UNLOCK_FILES){
			retval = unlockFile(token);
			if(config.verbose) check_errno(token, retval, SET_LOCK, 0);
			if(config.verbose && retval >= 0) printf("Unlocked %s\n", token);
		}
		else if(command & DELETE_FILES){ 
			retval = removeFile(token);
			if(config.verbose) check_errno(token, retval, REMOVE, 0);
			if(config.verbose && retval >= 0) printf("Rimosso %s\n", token);
		}
		if(real_path){
			free(real_path);
			real_path = NULL;
		} 
		token = strtok_r(NULL, DELIM, &tmpstr);
	}
	return 0;
}

int recursive_visit(char *start_dir, int files_to_write, bool locked, const char* dirname){
	DIR* target_dir = NULL;
	struct stat dirent_info, st;
    struct dirent* current_file;
	char real_path[PATH_MAX] = {0};
	char file_path[PATH_MAX] = {0};
	int files_written = 0, rec_visit = 0, open_file_val = 0, file = 0, retval = 0;
	size_t size = 0;
	unsigned char *buffer = NULL;
	if((target_dir = opendir(start_dir)) == NULL){
		return -1;
	}
	errno = 0;
	while((current_file = readdir(target_dir)) && (!files_to_write || files_written < files_to_write)){
		memset(file_path, 0, sizeof file_path);
		memset(real_path, 0, sizeof real_path);
		if(errno && !current_file) return -1; 
		if(strcmp(current_file->d_name, ".") == 0 || strcmp(current_file->d_name, "..") == 0) continue;
		snprintf(file_path, PATH_MAX-1, "%s/%s", start_dir, current_file->d_name);
		if(!realpath(file_path, real_path)){
			if(config.verbose) perror("Errore realpath!");
			continue;
		}
	 	if (stat(real_path, &dirent_info) == -1) {
            if(config.verbose) perror("Errore recupero informazioni del file!");
			continue;
        }
		if(S_ISDIR(dirent_info.st_mode)){
			rec_visit = recursive_visit(real_path, files_to_write - files_written, locked, dirname);
			if(rec_visit < 0) return -1;
			files_written += rec_visit;
		}
		else{
			errno = 0;
			open_file_val = openFile(real_path, O_CREATE | O_LOCK);
			if(open_file_val >= 0){
				retval = writeFile(real_path, dirname);
				if(config.verbose) check_errno(real_path, retval, WRITE, 0);
				if(!locked){
					retval = unlockFile(real_path);
					if(config.verbose) check_errno(real_path, retval, SET_LOCK, 0);
				} 
				retval = closeFile(real_path);
				if(config.verbose) check_errno(real_path, retval, CLOSE, 0);
				stat(real_path, &st);
				if(config.verbose && retval >= 0) printf("Scritti %lu bytes (file: %s)\n", st.st_size, real_path);
			}
			else if(open_file_val <= 0 && errno == EEXIST){
				errno = 0;
				if(openFile(real_path, 0) < 0){
					if(config.verbose){
						fprintf(stderr, ANSI_COLOR_RED"Errore openFile -> %s"ANSI_COLOR_RESET_N, strerror(errno));
						print_errno_summary();
					} 
					continue;
				}
				if((file = open(real_path, O_RDONLY)) == -1){
					if (config.verbose) perror("Errore durante l'apertura del file in locale");
					continue;
				}
				if(fstat(file, &st) < 0){
					if (config.verbose) perror("Errore fstat");
					continue;
				}
				size = st.st_size;
				buffer =  (unsigned char *) calloc(size, sizeof(unsigned char));
				if(read(file, buffer, size) < 0){
					if(config.verbose) CHECKERRNO(true, "Errore read dal file locale");
					free(buffer);
					buffer = NULL;
				}
				retval = appendToFile(real_path, buffer, size, dirname);
				if(config.verbose) check_errno(real_path, retval, APPEND, 0);
				retval = closeFile(real_path);
				if(config.verbose) check_errno(real_path, retval, CLOSE, 0);
				if(config.verbose && retval >= 0) printf("Appended %lu bytes (file: %s)\n", st.st_size, real_path);
				free(buffer);
				buffer = NULL;
			}
			else{
				if(config.verbose) check_errno(real_path, open_file_val, OPEN, 0);
				continue;
			}
			files_written++;
		}
	}
	if (closedir(target_dir) == -1) {
		if(config.verbose) perror("Errore chiusura cartella!");
		return -1;
	}
	return files_written;
}

int write_dir(char *args, bool is_locked, const char* dirname){
	int N = 0, files_written = 0;;
	char *dirname_to_write = NULL;
	char *n = NULL;
	char *tmpstr = NULL;
	char *real_path = NULL;
	dirname_to_write = strtok_r(args, DELIM, &tmpstr);
	real_path = realpath(dirname_to_write, NULL);
	if(!real_path){
		if (config.verbose) fprintf(stderr, ANSI_COLOR_RED"Cartella non valida!"ANSI_COLOR_RESET_N);
		return -1;
	}
	if((n = strtok_r(NULL, DELIM, &tmpstr))){
		errno = 0;
		N = strtol(n, NULL, 10);
		if(errno){
			if (config.verbose) perror("Errore: n non è valido! Default n = 0");
			N = 0;
		}
	}
	files_written = recursive_visit(real_path, N, is_locked, dirname);
	if(files_written < 0) return -1;
	else if(config.verbose) printf("Scritti %d files!\n", files_written);
	free(real_path);
	real_path = NULL;
	return 0;
}

void do_work(work_queue **head, work_queue **tail){
	unsigned char command = 0;
	char* args = NULL;
	char* dirname = NULL;
	int N = 0, files_read = 0;
	bool is_locked = true;
	while((*tail)){
		dequeue_work(&command, &args, &dirname, &is_locked, head, tail);
		if(command & WRITE_DIR) write_dir(args, is_locked, dirname);
		else if(command & READ_FILES) handle_read_files(args, dirname);
		else if(command & READ_N_FILES){
			errno = 0;
			N = strtol(args, NULL, 10);
			if(errno != 0){
				if (config.verbose) perror("Il numero inserito non è valido!");
				errno = 0;
				continue;
			}
			files_read = readNFile(N, dirname);
			if(config.verbose) printf("Letti %d files\n", files_read);
		}
		else handle_simple_request(args, command, dirname);
		free(args);
		args = NULL;
		if(dirname){
			free(dirname);
			dirname = NULL;
		}
		usleep(config.interval * 1000);
	}
}

void enqueue_work(unsigned char command, char *args, work_queue **head, work_queue **tail){
	work_queue* new = (work_queue*) malloc(sizeof(work_queue));
	CHECKALLOC(new, "Errore inserimento nella lista pronti");
	new->command = command;
	new->args = (char *) calloc((strlen(args)+1), sizeof(char));
	strcpy(new->args, args);
	new->is_locked = true;
	new->next = (*head);
	new->working_dir = NULL;
	new->prev = NULL;
	if((*tail) == NULL)
		(*tail) = new;
	if((*head) != NULL)
		(*head)->prev = new;
	(*head) = new;	
} 

int dequeue_work(unsigned char* command, char **args, char **dirname, bool *is_locked, work_queue **head, work_queue **tail){
	work_queue *befree = NULL;
	if((*tail) == NULL)
		return -1;

	*command = (*tail)->command;
	*args = (char *) calloc((strlen((*tail)->args))+1, sizeof(char));
	*is_locked = (*tail)->is_locked;
	strcpy(*args, (*tail)->args);
	if((*tail)->working_dir){
		*dirname = (char *)calloc((strlen((*tail)->working_dir)) + 1, sizeof(char));
		strcpy(*dirname, (*tail)->working_dir);
		free((*tail)->working_dir);
		(*tail)->working_dir = NULL;
	}
	free((*tail)->args);
	befree = (*tail);
	if((*tail)->prev != NULL)
		(*tail)->prev->next = NULL;
	
	if(((*tail) = (*tail)->prev) == NULL)
		(*head) = NULL;
	free(befree);
	return 0;
} 