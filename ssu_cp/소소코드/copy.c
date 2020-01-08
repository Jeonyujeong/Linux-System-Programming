#include <dirent.h>
#include <pwd.h>
#include <sys/wait.h>
#include "head.h"

extern int flag_s, flag_i, flag_l, flag_n, flag_p, flag_r, flag_d;

int isfile(char *fname) {
	struct stat file_info;

	// file 정보 stat에 저장 에러시 에러처리
	if (stat(fname, &file_info) < 0) {
		return -1;
	}

	// file 이면 return 1
	if (S_ISREG(file_info.st_mode)) {
		return 1;
	}
	// directroy 이면 return 0
	else if (S_ISDIR(file_info.st_mode)) {
		return 0;
	}
	else
		return -1;
}

void filecopy(char *source, char *target) {
	/*
	existcheck: target에 해당하는 파일이 있는지 확인하기 위한 변수	
	yn: 덮어쓰기 변경여부를 저장하기 위한 변수 
	*/
	int fd1, fd2, length, existcheck;
	char buf[BUFFER_SIZE], yn;
	yn = 'y';

	// source 오픈
	if ((fd1 = open(source, O_RDONLY)) < 0) {
		fprintf(stderr, "ssu_cp: %s: ", source);
		perror("");
		exit(1);
	}

	// -i option & -n option
	existcheck = isfile(target);
	// file이 존재할 때
	if (existcheck == 1) {
		if (flag_i) {
			printf("overwrite %s (y/n)? ", target);
			scanf("%c", &yn);
		}
		if (flag_n)
			yn = 'n';
	}
	else if (existcheck == 0) {
		fprintf(stderr, "ssu_cp: %s: target is directoroy\n", target);
		usage();
		exit(1);
	}
	
	// overwrite 하지 않을 경우 수행되지 않음
	if (yn == 'y' || yn == 'Y'){
		// target 오픈
		if ((fd2 = open(target,	O_RDWR| O_CREAT| O_TRUNC, 0644)) < 0) {
			fprintf(stderr, "ssu_cp: %s: ", target);
			perror("");
			usage();
			exit(1);
		}
		// source를 target에 복사
		while ((length = read(fd1, buf, BUFFER_SIZE)) > 0) {
			write(fd2, buf, length);
		}
	}

	// file close
	close(fd1);
	close(fd2);

	// file info copy
	if (flag_l)
		infoCopy(source, target);

}

void symcopy(char *source, char *target) {
	/*existcheck: target에 해당하는 파일이 있는지 확인하기 위한 변수*/
	int existcheck;

	// target이 존재하면 삭제
	existcheck = isfile(target);
	if (existcheck == 1) {
		remove(target);
	}

	// -s option 일 때 symlink. 에러시 에러처리
	if (symlink(source, target) < 0) {
		fprintf(stderr, "ssu_cp: %s: ", target);
		perror("");
		usage();
		exit(1);
	}
}

void dircopy(char *source, char*target) {
	/*
	filedir: 파일/디렉터리를 구별하기 위한 변수
	existcheck: 디렉터리가 존재하는지 확인하기 위한 변수
	tarpath: 복사될 파일의 경로/파일이름
	sorpath: 복사할 파일의 경로/파일 이름
	yn: 덮어쓰기를 수행 여부를 입력받는 변수
	*/
	DIR *dp;
	struct dirent *dentry;
	struct stat statbuf;
	int filedir, pid, status, existcheck;
	char *ptr, yn;
	char tarpath[PATH_MAX], sorpath[PATH_MAX], path[PATH_MAX];
	yn = 'y';

	// source directory open
	if ((dp = opendir(source)) == NULL) {
		fprintf(stderr, "opendir error for %s\n", source);
		exit(1);
	}

	// target이 이미 존재하는 지 check
	existcheck = isfile(target);
	if (existcheck == 1) {
			fprintf(stderr, "ssu_cp: %s: target is file\n", target);
			usage();
			exit(1);
	}
	else if (existcheck == 0) {
		// -i option 일 경우 overwrite여부 입력받기
		if (flag_i) {
			printf("overwrite %s (y/n)? ", target);
			scanf("%c", &yn);
			flag_i = 0;
		}
		// -n option 일 경우 yn='n'설정
		if (flag_n)
			yn = 'n';
	}
	// target이 존재하지 않으면 만들기
	else 
		mkdir(target, 0776);

	// overwrite하지 않을 경우 그냥 종료
	if (yn != 'y' && yn != 'Y') {
		exit(1);
	}

	// -l option 일 경우 파일 정보까지 복사
	if(flag_l)
		infoCopy(source, target);

	while ((dentry = readdir(dp)) != NULL) {
		// "." 와 ".." 를 제외하기
		if (strcmp(dentry->d_name, ".") && strcmp(dentry->d_name, "..")) {

			// 경로 저장
			if (getcwd(path, PATH_MAX) == NULL)
				perror("getcwd");

			// 복사될 파일의 경로 tarpath = taget/d_name
			strcpy(tarpath, target);
			ptr = tarpath + strlen(tarpath);
			*ptr++ = '/';
			*ptr = '\0';
			strcat(tarpath, dentry->d_name);

			// source로 경로 이동하기
			chdir(source);
			// 복사할 것이 파일인지 디렉터리인지 구분하기
			filedir = isfile(dentry->d_name);
			// 복사할 파일의 경로 sorpath = source/d_name
			strcpy(sorpath, source);
			ptr = sorpath + strlen(sorpath);
			*ptr++ = '/';
			*ptr = '\0';
			strcat(sorpath, dentry->d_name);
			chdir(path);


			// 파일이라면
			if (filedir == 1) {
				filecopy(sorpath, tarpath);
			}		
			// 디렉터리라면
			else if (filedir == 0) {
				dircopy(sorpath, tarpath);
			}

		}

	}
	
	// directory close
	closedir(dp);
}

void dirfork(char *source, char *target, int maxfork) {
	/*
	maxfork: 실행시 입력으로 받은 N값
	dircheck: 파일/디렉터리를 구별하기 위한 변수
	existcheck: 디렉터리가 존재하는지 확인하기 위한 변수
	count: source디렉터리에 있는 파일/디렉터리 목록의 개수
	dircnt: 하위디렉터리 개수
	forkcnt: 생성된 fork 개수 
	namelist: source디렉터리에 있는 파일/디렉터리의 목록
	tarpath: 복사될 파일의 경로/파일이름
	sorpath: 복사할 파일의 경로/파일 이름
	curpath: 현재 경로
	yn: 덮어쓰기를 수행 여부를 입력받는 변수
	*/
	int count, dircheck, status, dircnt, existcheck;
	int i, forkcnt=0;
	int pid[maxfork];
	struct dirent **namelist;
	char *ptr, yn;
	char sourpath[PATH_MAX];
	char tarpath[PATH_MAX];
	char curpath[PATH_MAX];
	yn = 'y';

	// source directory에 있는 파일/디렉터리 정보를 namelist에 저장
	if ((count = scandir(source, &namelist, NULL, alphasort)) == -1) {
		fprintf(stderr, "ssu_cp: %s: ", source);
		perror("");
		usage();
		exit(1);
	}
	
	// 파일,디렉터리 체크를 위해 해당 디렉터리로 이동
	getcwd(curpath, PATH_MAX);
	chdir(source);
	// '.' 과 '..'을 제외하고 디렉터리의 개수 세기
	for (i = 0; i < count; i++) {
		if (strcmp(namelist[i]->d_name, ".") && strcmp(namelist[i]->d_name, "..")) {
		dircheck = isfile(namelist[i]->d_name);
		if (dircheck == 0) 
			dircnt++;
		}
	}
	// 원래 경로로 돌아오기
	chdir(curpath);

	// source 안에 들어있는 디렉터리 개수가 입력받은 N의 보다 작으면 에러처리
	if (dircnt < maxfork) {
		fprintf(stderr, "ssu_cp: N should less than directory count\n");
		usage();
		exit(1);
	}

	// target이 이미 존재하는 지 check
	existcheck = isfile(target);
	if (existcheck == 1) {
		fprintf(stderr, "ssu_cp: %s: target is file\n", target);
		usage();
		exit(1);
	}
	else if (existcheck == 0) {
		// -i option 일 경우 overwrite여부 입력받기
		if (flag_i) {
			printf("overwrite %s (y/n)? ", target);
			scanf("%c", &yn);
			flag_i = 0;
		}
		// -n option 일 경우 yn='n'설정
		if (flag_n)
			yn = 'n';
	}
	// target이 존재하지 않으면 만들기
	else 
		mkdir(target, 0776);

	// overwrite하지 않을 경우 그냥 종료
	if (yn != 'y' && yn != 'Y') {
		exit(1);
	}

	// -l option 일 때 파일 정보까지 복사
	if (flag_l) 
		infoCopy(source, target);
	
	for (i = 0; i < count; i++) {
		// "." & ".." 제외하고 찾기
		if (strcmp(namelist[i]->d_name, ".") && strcmp(namelist[i]->d_name, "..")) {
			// sourpath = source/namelist[i]->d_name 으로 경로 만들기
			strcpy(sourpath, source);
			ptr = sourpath + strlen(sourpath);
			*ptr++ = '/';
			*ptr = '\0';
			strcat(sourpath, namelist[i]->d_name);

			// tarpath = target/namelist[i]->d_name 으로 경로 만들기
			strcpy(tarpath, target);
			ptr = tarpath + strlen(tarpath);
			*ptr++ = '/';
			*ptr = '\0';
			strcat(tarpath, namelist[i]->d_name);

			// 디렉터리인지 체크
			dircheck = isfile(sourpath);
			if (dircheck == 0) {
				// fork개수가 makfork개수를 넘지 않을때 까지
				if (forkcnt < maxfork) {
					pid[forkcnt] = fork();
					// 자식 프로세스
					if (pid[forkcnt] == 0) {
						// 자식 프로세스가 복사한 디렉터리와 해당 프로세스 pid 출력
						printf("getpid = %d, dir = %s\n", getpid(), namelist[i]->d_name);
						dircopy(sourpath, tarpath);
						exit(1);
					}
					// 부모 프로세스
					else if (pid[forkcnt] > 0) {

					}
					else 
						fprintf(stderr, "fork error\n");
					
					// fork 개수 세기
					forkcnt++;
				}
				// fork 개수보다 하위디렉터리가 많을 경우 부모 프로세스가 수행함
				else {
					// 부모 프로세스가 복사한 디렉터리와 해당 프로세스 pid 출력
					printf("getpid = %d, dir = %s\n", getpid(), namelist[i]->d_name);
					dircopy(sourpath, tarpath);
				}
			}
			// 파일이면 부모프로세스가 수행됨
			else {
				filecopy(sourpath, tarpath);
			}
		}
	}

	// 자식프로세스가 종료될 때 까지 기다린다.
	for (i = 0; i < forkcnt; i++) {
		waitpid(pid[i], &status, 0);
	}

	// namelist 할당된 메모리 초기화
	for (i = 0; i < count; i++)
		free(namelist[i]);
	free(namelist);

}
