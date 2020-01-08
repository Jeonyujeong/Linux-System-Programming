#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <sys/times.h>
#include <time.h>
#include <pthread.h>
#include <wait.h>

#define LENGTH_LIMIT 1024
#define BUFFER_SIZE 1024

char workpath[LENGTH_LIMIT];
int flag_c, flag_r, flag_m, flag_n, flag_d;
int period, tcnt, oldtcnt, backupN;
pthread_mutex_t mutex_lock;
pthread_t tid[50];
char tharg[50][LENGTH_LIMIT];
time_t addfilecheck;

int ssu_daemon_init(void);
int daemon_check(void);
static void my_signal_handler(int signo);
static void backup_log_time(char *time_log);
void option(int argc, char *argv[]);
void source_path(const char *filename, char *srcpath);
void target_path(const char *abfilename, char *tarpath);
void backup_exe(char *srcpath, char *tarpath);
time_t backup_log(char *pathname, char *filename, time_t intertime);
int modify_check(char *pathname, time_t *intertime);
int hexcmp(char *hexname, char *htname);
void name_change(const char *pathname, char *htname, char *listname);
void recovery_select(char *filename);
void recovery_write(const char *originalfile, char *recoveryfile);
int filetype(char *filename);
void directory_init(char *dir);
void *pthread_func(void *arg);
void create_pthread(void);
void directory_check(char *dir);
void write_filename(char *path, char *file);
void remove_oldfile(char *pathname);
int find_file(char *pathname, char (*filelist)[LENGTH_LIMIT], int *file_time);
void diff_execl(char *pathname);

int main(int argc, char* argv[])
{
	pid_t pid;
	int expid, filecheck;
	char filename[LENGTH_LIMIT];
	char srcpath[LENGTH_LIMIT];
	char tarpath[LENGTH_LIMIT];
	time_t intertime=0;
	int i, status;

	// 실행시 입력 처리
	if (argc < 3) {
		fprintf(stderr, "usage: %s <FILENAME> [PERIOD] [OPTION]\n", argv[0]);
		exit(1);
	}

	option(argc, argv);
	// input parsing (option, workpath, filename, period) store
	getcwd(workpath, sizeof(workpath));
	strcpy(filename, argv[optind]);
	// r, c option 에서는 period를 받지 않음.

	if (!flag_r && !flag_c) {
		period = atoi(argv[optind+1]);
		if (period < 3 || period > 10) {
			fprintf(stderr, "ssu_backup: N ranges 3 <=N<= 10\n");
			exit(1);
		}
	}
	
	if (flag_r || flag_c) {
		if (argc > 3) {
			fprintf(stderr, "ssu_backup: Invalid use.\n");
			exit(1);
		}
	}

//	printf("backupN: %d, period:%d\n", backupN, period);
	// 실행중인 데몬 프로세스 확인하기, 있다면 SIGUSR1 시그널 보내기
	if ((expid = daemon_check()) > 0) {
		printf("Send signal to ssu_backup process<%d>\n", expid);
		kill(expid, SIGUSR1);
	}

	// file type check (regular, directory, etc)
	filecheck = filetype(filename);
	if (filecheck < 0) {
		fprintf(stderr, "%s is not regular file\n", filename);
		exit(1);
	}
	else if (filecheck == 1) {
		if (flag_d) {
			fprintf(stderr, "regular file is not used -d option\n");
			exit(1);
		}
		source_path(filename, srcpath);
		target_path(srcpath, tarpath);
	}
	else if (filecheck == 0) {
		if (!flag_d) {
			fprintf(stderr, "directory is must used -d option\n");
			exit(1);
		}
	}	

	// backup directory creat
	mkdir("backup", 0775);

	// r option 일때
	if (flag_r) {
		recovery_select(filename);
		exit(0);
	}
	// c option 일 때
	else if (flag_c) {
		diff_execl(srcpath);
		exit(0);
	}

	// daemon process init
	printf("Daemon process initialization\n");
	if (ssu_daemon_init() < 0) {
		fprintf(stderr, "ssu_daemon_init failed\n");
		exit(1);
	}


	// filename이 directory 일 때
	if (flag_d) {
		// directroy에 대해 초기작업, 각각의 파일에 대해 thread 생성
		directory_init(filename);
		create_pthread();
		// directory에 수정사항이 있는지 체크 
		// 새로운 파일이 생성되었다면 그 파일에 대해 thread 생성
		while(1){
			directory_check(filename);
			create_pthread();
			sleep(period);
		}
	}
	// filename이 regular 일 때
	else {
		while (1) {
			if (flag_m) {
				// srcpath : 입력된 파일이름의 절대경로화
				// tarpath : 백업파일의 절대경로화
				if (modify_check(srcpath, &intertime)) {
					target_path(srcpath, tarpath);
					intertime = backup_log(srcpath, filename, intertime);
					if (intertime == 0)
						break;
					backup_exe(srcpath, tarpath);
					if (flag_n)
						remove_oldfile(srcpath);
					sleep(period);
				}
			}
			else {
				target_path(srcpath, tarpath);
				intertime = backup_log(srcpath, filename, intertime);
				if (intertime == 0) 
					break;
				backup_exe(srcpath, tarpath);
				if (flag_n)
					remove_oldfile(srcpath);
				sleep(period);
			}
		}
	}
	exit(0);
}

void option(int argc, char *argv[]) {
	int c;

	while ((c = getopt(argc, argv, "n:rcdm")) != -1) {
		switch (c) {
			case 'r':
				flag_r = 1;
				break;
			case 'c':
				flag_c = 1;
				break;
			case 'm':
				flag_m = 1;
				break;
			case 'n':
				backupN = atoi(optarg);
				if (backupN < 1){
					fprintf(stderr, "ssu_backup: -n option must be followed by a count.\n");
					exit(1);
				}
				flag_n = 1;
				break;
			case 'd':
				flag_d = 1;
				break;
			default:
				exit(1);
				;
		}
	}
	if (flag_c) {
		if (flag_r | flag_d | flag_m | flag_n) {
			fprintf(stderr, "ssu_backup: -r and -c options are not available with other options\n");
			exit(1);
		}
	}
	if (flag_r) {
		if (flag_c | flag_d | flag_m | flag_n) {
			fprintf(stderr, "ssu_backup: -r and -c options are not available with other options\n");
			exit(1);
		}
	}

}

void diff_execl(char *pathname) {
	char filelist[50][LENGTH_LIMIT];
	int filetime[50];
	char filename[LENGTH_LIMIT];
	char tarpath[LENGTH_LIMIT];
	char time[LENGTH_LIMIT];
	int count, max;
	int i, status, pid;

	// count = pathname과 같은 이름의 file의 개수
	// filetime 배열에 파일의 시간 저장
	// filelist 배열에 같은 이름의 파일들 저장
	count = find_file(pathname, filelist, filetime);

	max = 0;
	// 최신 파일 고르기
	for (i = 0 ; i < count; i++)  {
		if (filetime[i] > filetime[max])
			max = i;
	}

	// filename 은 순수한 경로를 제외한 파일이름
	// filename_filetime
	write_filename(pathname, filename);
	sprintf(time, "_0%d", filetime[max]);
	if (strlen(time) > 11)
		sprintf(time, "_%d", filetime[max]);
	strcat(filename, time);

	printf("<Compare with backup file[%s]>\n", filename);

	// tarpath 는 최신파일의 경로
	sprintf(tarpath, "%s/%s/%s", workpath, "backup", filelist[max]);
	if (fork() == 0) 
		execl("/usr/bin/diff", "diff", pathname, tarpath , NULL);

	wait(&status);

}

int find_file(char *pathname, char (*filelist)[LENGTH_LIMIT], int *filetime) {
	struct dirent **namelist;
	int i,j,k,l, count, min, check;
	char dirpath[LENGTH_LIMIT] = {0};
	char tmp[LENGTH_LIMIT];
	char filetmp[LENGTH_LIMIT];
	char hexname[LENGTH_LIMIT]={0};
	char token[LENGTH_LIMIT]={0};

	// dirpath : backup directory의 경로
	source_path("backup", dirpath);
	if ((count = scandir(dirpath, &namelist, NULL, alphasort)) < 0) {
		syslog(LOG_ERR, "directory_check scandir %s %m", dirpath);
		exit(1);
	}

	// hexname : pathname의 16진수화
	for (i = 0; pathname[i]!= '\0'; i++) {
		sprintf(token, "%x", pathname[i]);
		strcat(hexname, token);
	}

	k=0; check=0; l=0;
	// filelist : backup 디렉터리에 있는 pathname과 같은 파일 저장
	// filetime : 같은 이름의 파일들의 시간 저장
	for (i = 0; i < count; i++) {
		if (strcmp(namelist[i]->d_name, ".")&& strcmp(namelist[i]->d_name, "..")) {
			for (j = 0; namelist[i]->d_name[j] != '\0'; j++) {
				tmp[k] = namelist[i]->d_name[j];
				k++;
				if (namelist[i]->d_name[j] == '_') {
					tmp[k-1] = '\0';
					strcpy(filetmp, tmp);
					// 같은 이름의 파일을 filelist에 저장
					if (!strcmp(filetmp, hexname)) {
						check = 1;
						strcpy(filelist[l], namelist[i]->d_name);
					}
					k = 0;
				}
			}
			// file의 시간 저장
			if (check) {
				tmp[k] = '\0';
				filetime[l] = atoi(tmp);
				l++;
			}
			check = 0;
			k = 0;
		}
	}

	for (i=0; i< count; i++) 
		free(namelist[i]);
	free(namelist);

	// 같은 파일의 개수 리턴
	return l;
}

void remove_oldfile(char *pathname) { 
	int file_time[50];
	int i,j,k,l, count, min, check;
	char removefile[LENGTH_LIMIT]={0};
	char filelist[50][LENGTH_LIMIT];
	char tmp[BUFFER_SIZE];
	char dirpath[LENGTH_LIMIT];

	// count = pathname과 같은 이름의 file의 개수
	// file_time 배열에 파일의 시간 저장
	// filelist 배열에 같은 이름의 파일들 저장
	count = find_file(pathname, filelist, file_time);
	l = count;
	min = 0;
	// 백업 파일이 backupN보다 많으면 삭제
	while (l > backupN) {
		// 가장 오래된 파일 찾기
		for (i = 0; i < count; i++) {
			if (file_time[i] > 0)
				if (file_time[i] < file_time[min])
					min = i;
		}

		// dirpath : backup 디렉터리의 경로
		// removefile : 가장 오래된 파일의 경로
		source_path("backup", dirpath);
		sprintf(removefile,"%s/%s", dirpath, filelist[min]);
		backup_log(removefile, pathname, -1);
		// 오래된 파일 삭제
		if (remove(removefile) < 0) {
			syslog(LOG_ERR, "remove error %s %m", removefile);
			exit(1);
		}
		// 삭제된 파일의 시간 -1로 변경
		file_time[min] = -1;
		l--;
		min++;
	}
}

void directory_check(char *dir) {
	struct dirent **namelist;
	struct stat statbuf;
	int i,j, count, check;
	char dirpath[LENGTH_LIMIT] = {0};
	char catdir[LENGTH_LIMIT] = {0};

	// dirpath : 입력받은 디렉터리의 경로
	source_path(dir, dirpath);
	if ((count = scandir(dirpath, &namelist, NULL, alphasort)) < 0) {
		syslog(LOG_ERR, "directory_check scandir %s %m", dirpath);
		exit(1);
	}

	for (i = 0; i < count; i++) {
		if (strcmp(namelist[i]->d_name, ".")&& strcmp(namelist[i]->d_name, "..")) {
			chdir(dirpath);
			if (lstat(namelist[i]->d_name, &statbuf) < 0) {
				syslog(LOG_ERR, "directory_check lstat %m");
				exit(1);
			}
			// catdir : 디렉터리 안에 있는 파일의 경로 (workpath/dir/filename)
			sprintf(catdir, "%s/%s", dir, namelist[i]->d_name);
			// regular 파일일 때
			if (S_ISREG(statbuf.st_mode)) {
				// tharg : 각각의 thread에서 실행하는 파일 배열
				// 이미 존재하는 파일
				for (j = 0; j < tcnt; j++) {
					if (!(check = strcmp(tharg[j], catdir))){
						break;
					}
				}
				// 새로 생긴 파일
				// addfilecheck : 새로 생긴 파일이 있으면 1
				if (check) {
					strcpy(tharg[tcnt], catdir);
					addfilecheck = 1;
					tcnt++;
				}
			}
			// directory 일 때 재귀
			else if (S_ISDIR(statbuf.st_mode)) 
				directory_check(catdir);

			chdir(workpath);
		}
	}

	for (i = 0; i < count; i++) 
		free(namelist[i]);
	free(namelist);

}

void directory_init(char *dir) {
	struct stat statbuf;
	struct dirent **namelist;
	int count, i, status;
	char catdir[LENGTH_LIMIT]={0};
	char dirpath[LENGTH_LIMIT]= {0};

	// dirpath : 입력받은 디렉터리의 경로
	source_path(dir, dirpath);
	if ((count = scandir(dirpath, &namelist, NULL, alphasort)) < 0) {
		syslog(LOG_ERR, "directory_init scandir %s %s %m",dir, dirpath);
		exit(1);
	}

	for (i = 0; i < count; i++) {
		if (strcmp(namelist[i]->d_name, ".") && strcmp(namelist[i]->d_name, "..")) {
			chdir(dirpath);
			if (lstat(namelist[i]->d_name, &statbuf) < 0) {
				syslog(LOG_ERR, "directroy_init lstaterror %s%s %m",dirpath,namelist[i]->d_name);
				exit(1);
			}
			// catdir : 디렉터리에 존재하는 파일 경로
			sprintf(catdir, "%s/%s", dir, namelist[i]->d_name);
			// tharg : 파일이라면, tharg배열에 저장
			if (S_ISREG(statbuf.st_mode)){
				strcpy(tharg[tcnt], catdir);
				tcnt++;
			}
			// 디렉터리일 때 재귀
			else if (S_ISDIR(statbuf.st_mode)) {
				directory_init(catdir);
			}
			chdir(workpath);
		}
	}
	for (i = 0; i < count; i++) 
		free(namelist[i]);
	free(namelist);
}

void create_pthread(void) {
	int i, status;

	// oldtcnt : directory_check하기 전에 가지고 있던 thread개수 
	// tcnt : directory_check 후 변화된 thread개수, 즉 실행중인 thread개수
	// tharg에 담긴 파일이름으로 thread만들기
	pthread_mutex_lock(&mutex_lock);	// lock
	for (i = oldtcnt; i < tcnt; i++) {
		if (pthread_create(&tid[i], NULL, pthread_func, (void *)tharg[i]) != 0) {
			syslog(LOG_ERR, "directory_init pthread error %m");
			exit(1);
		}
	}
	oldtcnt = tcnt;
	pthread_mutex_unlock(&mutex_lock);	//unlock
}

void *pthread_func(void *arg) {
	pthread_t tid;
	char srcpath[LENGTH_LIMIT];
	char tarpath[LENGTH_LIMIT];
	time_t intertime = addfilecheck;
	char *filename = (char *)arg;

	// srcpath : filename의 절대경로
	source_path(filename, srcpath);
	tid = pthread_self();
//	syslog(LOG_INFO, "pthread_func %u %s", (unsigned int)tid, filename);

	while(1) {
		if (flag_m) {
			// directory가 수정되고 있는지 확인
			if (modify_check(srcpath, &intertime)) {
				pthread_mutex_lock(&mutex_lock);	// lock
				target_path(srcpath, tarpath);	// backupfile path
				intertime = backup_log(srcpath, filename, intertime);	// log
				pthread_mutex_unlock(&mutex_lock);	//unlock
				// if file delete
				if (intertime == 0) {
					pthread_exit((void *)0);
				}
				backup_exe(srcpath, tarpath);	// backup
				if (flag_n)
					remove_oldfile(srcpath);
			}
		}
		else {
			pthread_mutex_lock(&mutex_lock);	//lock
			target_path(srcpath, tarpath);	// backupfile path
			intertime = backup_log(srcpath, filename, intertime);	//log
			pthread_mutex_unlock(&mutex_lock);	// unlock
			// if file delete
			if (intertime == 0) {
					pthread_exit((void *)0);
			}
			backup_exe(srcpath, tarpath);	// backup
			if (flag_n)
				remove_oldfile(srcpath);
		}	
		sleep(period);
	}
	exit(0);
}

int filetype(char *filename) {
	struct stat statbuf;

	// file 없으면 error
	if (lstat(filename, &statbuf) < 0) {
		fprintf(stderr, "ssu_backup: %s does not exist \n", filename);
		exit(1);
	}

	if (S_ISREG(statbuf.st_mode)) // 일반파일
		return 1;
	else if (S_ISDIR(statbuf.st_mode))	// 디렉터리
		return 0;
	else	// 그 외의 파일
		return -1;
}

void recovery_select(char *filename) {
	DIR *dp;
	struct dirent **namelist;
	int count;
	char *backupdir = "backup";
	int i, cnt=0, input;
	char tmp[LENGTH_LIMIT] = {0};
	char hexname[LENGTH_LIMIT]= {0};
	char backuplist[100][LENGTH_LIMIT]= {0};
	char hexlist[100][LENGTH_LIMIT]= {0};
	char pathname[LENGTH_LIMIT]= {0};
	char name[LENGTH_LIMIT];

	// pathname : filename의 절대경로
	source_path(filename, pathname);

	// hexname : pathname의 16진수화
	for (i = 0; i < strlen(pathname); i++) {
		sprintf(tmp, "%x", pathname[i]);
		strcat(hexname, tmp);
	}

	// backuplist[0]
	strcpy(backuplist[0], "exit");
	cnt++;

	if ((count = scandir(backupdir, &namelist, NULL, alphasort)) == -1) {
		fprintf(stderr, "directory scan error\n");
		exit(1);
	}

	// hexlist : hexname과 같은 파일이름의 backup file들
	// backuplist : 백업파일 리스트 
	// name_change() : filename_time으로 화면에 출력할 문자열로 바꾸기
	// write_filename() : file의 경로를 제외한 이름부분
	for (i = 0; i < count; i++) {
		if (strcmp(namelist[i]->d_name, ".") && strcmp(namelist[i]->d_name, "..")) {
			if (hexcmp(hexname, namelist[i]->d_name)) {
				strcpy(hexlist[cnt], namelist[i]->d_name);
				write_filename(filename, name);
				name_change(name, namelist[i]->d_name, backuplist[cnt]);
				cnt++;
			}
		}
	}

	for (i = 0; i < count; i++) 
		free(namelist[i]);
	free(namelist);

	// backupfile이 없으면 메시지 출력 후 종료
	if (cnt < 2) {
		fprintf(stderr, "ssu_backup: %s does not exist backup file. \n", filename);
		exit(1);
	}

	// backuplist 출력
	printf("<%s backup list>\n", name);
	for (i = 0; i < cnt; i++) {
		printf("%d. %s\n", i, backuplist[i]);
	}
	printf("input : ");
	scanf("%d", &input);

	// input받은 번호로 file 복구
	if (input == 0)
		return ;
	else {
		printf("Recovery backup file...\n");
		printf("[%s]\n", name);
		recovery_write(name, hexlist[input]);
	}

}

void recovery_write(const char *originalfile, char *recoveryfile) {
	int fd1, fd2;
	int length;
	char buf[BUFFER_SIZE];

	// backup디렉터리로 이동
	chdir("backup");
	
	// 복구파일 읽기 모드로 오픈
	if ((fd1 = open(recoveryfile, O_RDONLY))< 0) {
		fprintf(stderr, "open recoveryfile error\n");
		exit(1);
	}
	chdir(workpath);

	// 입력받은 파일 덮어쓰기 모드로 오픈
	if ((fd2 = open(originalfile, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
		fprintf(stderr, "open origianlfile error\n");
		exit(1);
	}

	// 내용 쓰기
	while ((length = read(fd1, buf, sizeof(buf))) > 0) {
		fwrite(buf, length, 1, stdout);
		write(fd2, buf, length);
	}

	close(fd1);
	close(fd2);

}

void name_change(const char *filename, char *htname, char *listname) {
	char times[LENGTH_LIMIT];
	char sizes[LENGTH_LIMIT];
	struct stat statbuf;
	int i=0, j=0;

	// htname : hexname의 time만 추출
	while (htname[i] != '_') 
		i++;
	for ( ; htname[i] != '\0' ; i++) {
		times[j] = htname[i];
		j++;
	}
	times[j] = '\0';

	// size 정보 저장
	chdir("backup");
	if (lstat(htname, &statbuf) < 0) {
		fprintf(stderr, "lstat error\n");
		exit(1);
	}
	sprintf(sizes, " [size: %ld]", statbuf.st_size);
	chdir(workpath);

	// listname : 화면에 출력한 backup file list의 이름 만들기
	strcpy(listname, filename);
	strcat(listname, times);
	strcat(listname, sizes);
}

int hexcmp(char *hexname, char *htname) {
	char rmtime[LENGTH_LIMIT];
	int i;

	// rmtime : time을 제거한 절대경로 16진수 부분만을 의미
	for (i = 0; htname[i] != '_'; i++) {
		rmtime[i] = htname[i];
	}
	rmtime[i] = '\0';

	// 파일이름이 같으면 1, 다르면 0 리턴
	if (!strcmp(hexname, rmtime))
		return 1;
	else 
		return 0;
}

int modify_check(char *pathname, time_t *intertime) {
	struct stat statbuf;

	if (lstat(pathname, &statbuf) < 0) {
//		syslog(LOG_ERR, "modify_check lstat error %m");
	}

	// intertime != starbuf.st_mtime 일 때만 수정한 것으로 1 리턴
	if (*intertime == 0) {
		*intertime = statbuf.st_mtime;
		return 0;
	}
	else {
		if (*intertime != statbuf.st_mtime)
			return 1;
		else 
			return 0;
	}
}	

void write_filename(char *pathname, char *filename) {
	int i, j=0;
	char tmp[LENGTH_LIMIT];

	// 경로를 제외한 파일이름만 추출
	for (i = 0; pathname[i] != '\0'; i++) {
		tmp[j]= pathname[i];
		j++;
		if (pathname[i] == '/') {
			j = 0; 
		}
	}
	strcpy(filename, tmp);
	filename[j] = '\0';
}

time_t backup_log(char *pathname, char *filename, time_t intertime) {
	struct stat statbuf;
	char *logfile = "backup_log.txt";
	char time_log[BUFFER_SIZE];
	char stat_log[BUFFER_SIZE];
	char mtime_log[BUFFER_SIZE];
	char info[BUFFER_SIZE];
	char ablogfile[LENGTH_LIMIT];
	char file_log[LENGTH_LIMIT];
	struct tm *t;
	FILE *fp;

	// file_log : 경로를 제외한 파일이름 부분
	write_filename(pathname, file_log);
	// ablogfile : backup_log.txt의 절대경로
	sprintf(ablogfile, "%s/%s", workpath, logfile);
	// 읽기쓰기 모드로 오픈, 파일이 없으면 생성
	if ((fp = fopen(ablogfile, "r+")) == NULL) {
		if ((fp = fopen(ablogfile, "w")) == NULL) {
			syslog(LOG_ERR, "backup_log fopen error: %m");
			exit(1);
		}
	}

	// time_log : 백업시간
	backup_log_time(time_log);

	// 파일 삭제 되었을 때
	if (lstat(pathname, &statbuf) < 0) {
		sprintf(info, "[%s] %s is deleted\n", time_log, file_log);
		fseek(fp, 0, SEEK_END);
		fwrite(info, strlen(info), 1, fp);
		fclose(fp);
		return 0;
	}

	// mtime_log 만들기, 
	t = localtime(&statbuf.st_mtime);
	sprintf(mtime_log, "%02d%02d %02d:%02d:%02d", \
			t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

	// stat_log 만들기 : size, mtime
	sprintf(stat_log, "size:%ld/mtime:%s", statbuf.st_size, mtime_log);

	if (intertime == 0) {	// 처음 수행되었을 때
		intertime = statbuf.st_mtime;
		sprintf(info, "[%s] %s [%s]\n", time_log, file_log, stat_log);
	}
	else if (intertime == -1) {	//  백업파일을 삭제 했을 때
		write_filename(filename, file_log);
		sprintf(info, "[%s] Delete backup [%s, size:%ld, btime:%s]\n", \
				time_log, file_log, statbuf.st_size, mtime_log);
	}
	else {	// 파일이 수정되었는지 확인
		if (intertime != statbuf.st_mtime) {
			sprintf(info, "[%s] %s is modified [%s]\n", time_log, file_log, stat_log);
			intertime = statbuf.st_mtime;
		}
		else 
			sprintf(info, "[%s] %s [%s]\n", time_log, file_log, stat_log);
	}
	// 로그파일에 쓰기
	// 수정된 시간 리턴
	fseek(fp, 0, SEEK_END);
	fwrite(info, strlen(info), 1, fp);
	fclose(fp);
	return intertime;
}

void backup_exe(char *srcpath, char *tarpath) {
	FILE *fp1, *fp2;
	char buf[BUFFER_SIZE];
	int length;

	// 입력받은 파일 읽기모드로 오픈
	if ((fp1 = fopen(srcpath, "r")) == NULL) {
		syslog(LOG_ERR, "open failed fp1 %m");
		exit(1);
	}

	// 백업파일에 쓰기모드로 오픈
	if ((fp2 = fopen(tarpath, "w+")) == NULL) {
		syslog(LOG_ERR, "open failed fp2 %m");
		exit(1);
	}

	// 백업파일에 쓰기
	while (fgets(buf, sizeof(buf), fp1) != NULL){
		fputs(buf, fp2);
	}
	fclose(fp1);
	fclose(fp2);
}

void source_path(const char *filename, char *srcpath) {
	// 입력받은 파일이 절대경로가 아니라면
	// 현재의 작업경로로 절대경로 만들어주기
	if (filename[0] != '/')
		sprintf(srcpath, "%s/%s", workpath, filename);
	else 
		strcpy(srcpath, filename);
}

void target_path(const char *abfilename, char *tarpath) {
	char tmp[LENGTH_LIMIT]= {0};
	char token[LENGTH_LIMIT]= {0};
	char hexname[LENGTH_LIMIT]= {0};
	int i;
	time_t btime;
	struct tm *t;

	// abfilenae : 절대경로
	// hexname :절대경로 파일이름을 16진수화
	strcpy(tmp, abfilename);
	for (i = 0; tmp[i] != '\0'; i++) {
		sprintf(token, "%x", tmp[i]);
		strcat(hexname, token);
	}
	// 백업시간
	time(&btime);
	t = localtime(&btime);
	sprintf(token, "_%02d%02d%02d%02d%02d", \
			t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
	strcat(hexname, token);

	// 백업파일대상이름이 255을 넘으면 에러처리
	if (strlen(hexname) > 255) {
		syslog(LOG_ERR, "target_path length error: %m");
		fprintf(stderr, "backup name too long\n");
		exit(1);
	}
	// 백업파일의 절대 경로 
	sprintf(tarpath, "%s/backup/%s", workpath, hexname);
}	

int ssu_daemon_init(void) {
	pid_t pid;
	int fd, maxfd;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error\n");
		exit(1);
	}
	else if (pid != 0) // parent process 종료
		exit(0);

	pid = getpid();
	printf("process %d running as ssu_backup daemon.\n", pid);
	setsid();
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	// SIGUSR1, 시그널을 받으면 my_signal_handler함수 실행
	signal(SIGUSR1, my_signal_handler);	
	maxfd = getdtablesize();

	// 부모의 filedes 모두 닫기
	for (fd = 0; fd < maxfd; fd++) 
		close(fd);

	umask(0);
	chdir("/");
	fd = open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
	return 0;
}

static void my_signal_handler(int signo) {
	FILE *fp;
	char *logfile = "backup_log.txt";
	char exit_log[LENGTH_LIMIT];
	char time_log[LENGTH_LIMIT];
	char ablogfile[LENGTH_LIMIT];

	// ablogfile : backup_log.txt의 절대경로
	sprintf(ablogfile, "%s/%s", workpath, logfile);

	// 읽기 쓰기로 오픈
	if ((fp = fopen(ablogfile, "r+")) == NULL) {
		fprintf(stderr, "fopen error\n");
		exit(1);
	}

	// time_log : 백업시간
	// 실행중이었던 디몬 프로세스 종료 로그 작성
	backup_log_time(time_log);
	sprintf(exit_log, "[%s] ssu_backup<pid:%d> exit\n", time_log, getpid());
	fseek(fp, 0, SEEK_END);
	fwrite(exit_log, strlen(exit_log), 1, fp);
	fclose(fp);

	// 죽음
	raise(SIGTERM);
}

static void backup_log_time(char *time_log) {
	time_t btime;
	struct tm *t;

	// time_log : 백업 시간 만들기
	time(&btime);
	t = localtime(&btime);
	sprintf(time_log, "%02d%02d %02d:%02d:%02d", \
			t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
}

int daemon_check(void) {
	DIR *dp;
	struct dirent *dentry;
	struct stat statbuf;
	char cmdline[LENGTH_LIMIT];
	char psname[LENGTH_LIMIT];
	int pid, length, fd;

	// 프로세스 번호가 있는 /proc 디렉터리 오픈
	if ((dp = opendir("/proc")) == NULL) {
		fprintf(stderr, "ssu_backup: daemon_check: opendir error\n");
		exit(1);
	}

	chdir("/proc");
	while ((dentry = readdir(dp)) != NULL) {
		if (strcmp(dentry->d_name, ".") && strcmp(dentry->d_name, "..")) {

			if (lstat(dentry->d_name, &statbuf) < 0){
				fprintf(stderr, "ssu_backup: daemon_check: lstat error\n");
				exit(1);
			}

			// 디렉터리가 아니면 넘기기
			if (!S_ISDIR(statbuf.st_mode))
				continue;

			// 디렉터리 이름이 번호가 아니면 넘기기
			if ((pid = atoi(dentry->d_name)) <= 0)
				continue;

			// cmdline : cmdline 파일의 경로
			// 프로세스 이름이 있는 /proc/pid/cmdline 오픈
			sprintf(cmdline, "/proc/%d/cmdline", pid);
			if ((fd = open(cmdline, O_RDONLY)) < 0) {
				fprintf(stderr, "ssu_backup: daemon_check: open error\n");
				exit(1);
			}

			// psname : 프로세스의 이름
			length = read(fd, psname, sizeof(psname));
			psname[length] = '\0';

			// ./ssu_bakcup 이름의 프로세스이름이 이미 존재한다면 
			// 해당 프로세스의 pid 리턴
			if (!strcmp(psname, "./ssu_backup") && (pid != getpid())) {
				chdir(workpath);
				return pid;
			}
		}
	}
	chdir(workpath);
	return -1;
}








