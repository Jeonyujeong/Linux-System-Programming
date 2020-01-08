#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>

#define BUFFER_SIZE 1024
#define N 100

struct macrovalue {
	char macro[BUFFER_SIZE];
	char value[BUFFER_SIZE];
};
struct component {
	char target[BUFFER_SIZE];
	char depend[N][BUFFER_SIZE];
	char command[N][BUFFER_SIZE];
	int dcnt;
	int ccnt;
};

struct macrovalue mv[N];
struct component com[N];
char pathname[N][BUFFER_SIZE];
int mcnt=0, tcnt=0, pcnt=0, bufcnt;
int tnumbuf[N];
int flag_s, flag_m;

void parsing (char *filename);		
void changeCommand(void);
void exeCommand(int tnum);
void errorCheck(char* fname);
void targetCheck(void);
int dependFind(int tnum);
int timeCheck(int tnum, int dnum);
int dirCheck(char* fname);
void includef(void);
void flag_h(void);
void macroList(void);
void inputMacro(char *argv);
void inputTarget(char *argv);
int main(int argc, char* argv[]) 
{
	char *fname = "Makefile";	// filename default
	int c, i, j, opcnt=0;		// opcnt: option으로 받은 argv[]개수
	int maxmc=0, maxtg=0;		// maxmc: 입력받은 macro개수, maxtg: 입력받은 target개수
	int ismacro;				// 매크로 인지 확인하기 위해
	char tmp[BUFFER_SIZE];		


	// option
	while ((c = getopt(argc, argv, "hc:f:ms")) != -1) {
		switch(c) {
			case 'h':
				flag_h();	// 함수 사용법 출력해주는 함수 호출
				break;
			case 'c':
				if (dirCheck(optarg) == 0) {	// 디렉토리가 없으면 에러처리
					fprintf(stderr, "make: %s: No such file or directory\n", optarg);
					exit(1);
				}
				chdir(optarg);	// 디렉터리 이동
				opcnt++;		// 옵션 뒤 별도의 파라미터가 있어서
				break;
			case 'f' :
				fname = optarg;	// filename 변경
				opcnt++;		// 옵션 뒤 별도의 파라미터가 있어서 
				break;	
			case 'm':
				flag_m = 1;
				break;
			case 's' :
				flag_s = 1;
				break;
			default :
				exit(1);
		}
		opcnt++;	
	}

	errorCheck(fname);
	
	// parsing
	parsing(fname);
	includef();

	targetCheck();
	changeCommand();

	// target, macro input
	for (i=opcnt+1; i<argc; i++) {
		ismacro = 0;	// macro인지 확인하기 위해
		for (j=0; argv[i][j] != '\0'; j++) {
			if (argv[i][j] == '=') {	//'='이 있다면 macro
				maxmc++;
				if (maxmc > 5) {	// 5개를 넘으면 에러처리
					fprintf(stderr, "macro max 5\n");
					exit(1);
				}
				inputMacro(argv[i]);
				ismacro = 1;
				break;
			}
		}
		if (!ismacro && !flag_m) {	//macro가 아니면 target, -m옵션이 없으면 실행 
			maxtg++;
			if (maxtg > 5) {
				fprintf(stderr, "target max 5\n");
				exit(1);
			}
			inputTarget(argv[i]);
		}
	}

	// macro print
	if (flag_m) {
		macroList();
	}

	// no target input
	if (maxtg == 0) {
		inputTarget(com[0].target);
	}


}

// input macro add, fix
void inputMacro(char *argv) {
	int i, j, c=0;
	char tmp[BUFFER_SIZE];
	int chvnum, change=0;	// chvnum: (change value num) 바꿀 value의 번호를 기억

	for (i=0; argv[i]!='\0'; i++) {
		tmp[c] = argv[i];
		if (argv[i] == '=') {	
			tmp[c] ='\0';
			c = -1;
			for (j=0; j<mcnt; j++) {
				if (!strcmp(tmp, mv[j].macro)) {	// 정의된 macro에 이미 이는지 확인
					change = 1;	// 바꿨음을 기억
					chvnum = j;	// 정의된 macro가 있으면 num을 기억
				}
				else {
					strcpy(mv[mcnt].macro, tmp);	// 없으면 추가
				}
			}
		}
		c++;
	}
	tmp[c] = '\0';
	if (change) {	// chvnum에 value값만 바꿔주기
		strcpy(mv[chvnum].value, tmp);
	}
	else {			// mv 구조체에 추가 
		strcpy(mv[mcnt].value, tmp);
		mcnt++;
	}
}

// target select
void inputTarget(char *argv) {
	int i, j, tgnum, update;	//tgnum: target의 number, update: command실행했는지 기억

	for (i=0; i<tcnt; i++) {
		if (!strcmp(argv, com[i].target)) {
			tgnum = i;
			if (dependFind(tgnum)==0) {	// 일치하는 target의 number로 재귀 호출
				if (!flag_s)	// -s 옵션이면 command출력 안하기 때문에
					fprintf(stderr, "make: '%s' is up to date\n", com[i].target);
			}
			return ;
		}
	}
	// 실행시 지정한 타겟이 없을 경우
	fprintf(stderr, "make: *** No rule to make target '%s'. Stop\n", argv);
	exit(1);

}

// usage, 사용법 출력 함수
void flag_h (void) {
	fprintf(stderr, "Usage : ssu_make [Target] [Option] [Macro]\n");
	fprintf(stderr, "Option:\n\
			-f <file>\tUse <file> as a makefile\n\
			-c <directory>\tChange to directory <directory> before reading the makefiles.\n\
			-s\t\tDo not print the commands as they are execuredt\n\
			-h\t\tprint usage\n\
			-m\t\tprint macro list\n\
			-t\t\tprint tree\n");
	exit(1);
}

// macro list print
void macroList (void) {
	int i;

	printf("-----------------macro list-----------------\n");
	for (i=0; i<mcnt; i++) {
		printf("%s-> %s\n", mv[i].macro, mv[i].value);
	}

	exit(1);
}

// parsing
void parsing (char *fname) {
	int fd;
	char filename[BUFFER_SIZE];
	int line=0, lstr=0, onestr=0;	// lstr: '\\'긴문장 처리할때 사용, onestr: '"'한문장일 때
	char ch, tmpc, bch;		//tmpc: 특정 기호 만났을 때 저장, bch: ch의 전의 문자
	char tmp[BUFFER_SIZE];	
	int m=0, t=0, c=0, d=0, p=0, i=0, j=0;
	char *ptr;	

	strcpy(filename, fname);

	// open 에러체크
	if ((fd = open(fname, O_RDONLY)) < 0) {
		fprintf(stderr, "make: '%s': No such file or directory\n", filename);
		exit(1);
	}

	tmpc = '\n';	
	
	// macro, value find
	while (read(fd, &ch, sizeof(char)) > 0) {
		if (ch != ' ' && ch != '\t' ) {	// 공백과 탭은 빼기
			tmp[i] = ch;	// 한 글자씩 저장
			i++;
			if (ch == '=') {	// '='이면 앞부분은 macro로 저장
				if (tmp[i-2] == '?') {
					i--;
				}
				tmp[i-1] = '\0';
				strcpy(mv[mcnt].macro, tmp);
				tmpc = ch;	// tmpc = '=' 
				i = 0;
			}
			else if (ch == '\n') {	// '\n'이면 한 문장 끝
				if (bch == '\\') {	// 긴문장 처리 문자를 만나면 lstr 1
					lstr = 1;
				}
				else {
					lstr = 0;
				}
				if (tmpc == '=') {	// tmpc가 '='이면 뒷부분이 value
					if (!lstr) {
						tmp[i-1] = '\0';
						strcpy(mv[mcnt].value, tmp);
						tmpc = ch;
						i = 0;
						m++;
						mcnt++;	// macro 개수 증가
					}
					else {
						i-=2;
					}

				}
				else {
					i = 0;
				}
				onestr = 0;
			}
			else if (ch == '"') {	// 한문장임을 기억해두기
				onestr = 1;
			}
		}
		if (lstr) {	// 긴문장이었으면 공백과 탭도 저장
			if (ch == ' ' || ch == '\t') {
				tmp[i] = ch;
				i++;
			}
		}
		else if (onestr) {	// 한문장이었으면 공백과 탭도 저장
			if (ch == ' ' || ch == '\t') {
				tmp[i] = ch;
				i++;
			}
		}
		bch = ch;	// 이전 문자 기억하기

	}


	// 오프셋 위치 이동 
	if(lseek(fd, 0, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		exit(1);
	}

	i = 0;
	tmpc = '\n';

	// target, dependecy, command find
	while(read(fd, &ch, sizeof(char)) > 0) {

		if (tmpc == '\n') {
			if (ch != ' ' && ch != '\t') {
				tmp[i] = ch;
				i++;
				if (ch == ':') {	//':' 만나면 앞부부은 target으로 저장
					tmp[i-1] = '\0';
					t++;
					tcnt++;
					c = 0;
					strcpy(com[tcnt-1].target, tmp);
					tmpc = ch;
					i = 0;
				}
				else if (ch == '\n') 
					i = 0;
			}
			else if (ch == '\t' && bch == '\n') {	//'\t'만나면 tmpc = '\t'
				tmpc = ch;
				i = 0;
			}
		}
		else if (tmpc == ':') {		
			tmp[i] = ch;
			if (ch == ' ' || ch == '\t' || ch == '\\') {	// 공백, 탭으로 구분 하기 위해
				if (i == 0) {	// 맨 앞부분 들어왔을 경우 처리
					i = -1;
				}
				else {
					tmp[i] = '\0';
					strcpy(com[tcnt-1].depend[d], tmp);	// dependency저장
					d++;
					com[tcnt-1].dcnt++;
					i = -1;
				}
			}
			else if (ch == '\n') {	// ch가 '\n'일 때
				if (bch == '\\') {	// 긴문장 처리를 위해
					lstr = 1;
				}
				else {
					lstr = 0;
				}

				if (!lstr) {
					tmp[i] = '\0';
					if (i != 0){ 
						strcpy(com[tcnt-1].depend[d], tmp); // 마지막에 있는 dependency 저장
						d++;
						com[tcnt-1].dcnt++;	// dependency개수 증가
					}
					i = -1;
					d = 0;
					tmpc = ch; // tmpc = '\n'
				}
				else 
					i--;
			}
			i++;
		}
		else if (tmpc == '\t') {	// '\t'이면 commmand처리
			tmp[i] = ch;
			if (ch == ' ' || ch == '\t') {	// 맨 앞에 나오는 공백과 탭 제거
				if (i == 0) {
					i = -1;
					tmpc = '\n';
				}
			}
			else if (ch == '\n') {	// 문장 끝에 왔을 때
				if (bch == '\\') {	// 긴문장인지 확인
					lstr = 1;
				}
				else {
					lstr = 0;
				}

				if (!lstr) {	// lstr이 0이면 command에 저장
					tmp[i] = '\0';
					strcpy(com[tcnt-1].command[c], tmp);
					c++;
					com[tcnt-1].ccnt++;
					i = -1;
					tmpc = ch;
				}
			}
			i++;
		}
		bch = ch;	// 이전 문자 기억
	}

	// 오프셋 이동
	if(lseek(fd, 0, SEEK_SET) < 0) {
		fprintf(stderr, "lseek error\n");
		exit(1);
	}

	i = 0;
	tmpc = '\n';

	// include find
	while(read(fd, &ch, sizeof(char)) > 0) {

		// 다음과 같은 기호를 만나면 tmpc가 변경됨
		if (ch == '='){
			tmpc = '=';
		}
		else if (ch == ':')
			tmpc = ':';
		else if (ch == '\t' && i==0){
			if (!lstr){
				tmpc = '\t';
			}
		}
		else if (ch == '#')
			tmpc = '#';
		
		// 한 줄의 끝에 왔는데 tmpc의 값이 '+' 아닐 때
		else if (ch == '\n' && tmpc != '+'){
			line++;

			// 긴문장 처리
			if (tmp[i-1] == '\\'){
				i--;
				continue;				
			}

			// 'include'도 아니고 규칙에 맞는 문법이 아니다
			if (tmpc == '-') {
				fprintf(stderr, "%s:%d: ***missing separator. Stop.\n", filename, line);
				exit(1);
			}
			tmpc = '\n';
			i = 0;
		}

		// 한 줄이 끝났는데 tmpc가 변경되지 않았다면
		if (tmpc == '\n'&& ch != '\n'){
			tmp[i] = ch;
			if(ch == ' ' || ch == '\t') {	// 공백과 탭으로 구분
				if (i==0) {		// 맨 앞에 공백과 탭이 들어오는 것을 처리
					i= -1;
				}
				else {
				tmp[i] = '\0';

				// 맨 처음 분리한 단어가 'include'와 같다면 '+'
				if(!strcmp(tmp, "include")) {
					tmpc = '+';
					i = -1;
				}
				else {	// 아니면 '-'
					tmpc = '-';
				}
				}
			}
			i++;
		}

		// include 였다면 
		else if (tmpc == '+') {
			tmp[i] = ch;
			if (ch == ' ' || ch == '\t') {
				if (i > 1){		// pathname 저장
					tmp[i] = '\0';
					strcpy(pathname[p], tmp);
					p++;
					pcnt++;
				}
				i = -1;
			}
			else if (ch == '\\') {	// 긴 문장 처리
				lstr = 1;
			}
			else if (ch == '\n') {	// 한 줄의 마지막에 왔다면
				if (tmp[i-1] == '\\') {	//'\\'있다면 없애주기 위해
					i -= 2;
				}
				else {
					if (i != 0){
						tmp[i] = '\0';
						strcpy(pathname[p], tmp);	// 마지막 추가 파일 저장
						p++;
						pcnt++;	//pathname개수
					}
					i = -1;
					lstr = 0;
					tmpc = ch;

				}
			}

			i++;
		}


	}
	
	/*
	   for (i=0; i<mcnt; i++) {
	   printf("%d'%s'='%s'\n",i, mv[i].macro, mv[i].value);
	   }


	   for (i=0; i<tcnt; i++) 
	   printf("%d[%s]\n",i, com[i].target);

	   for (i=0; i<tcnt; i++)
	   for (j=0; j<com[i].dcnt; j++)
	   printf("%d %d*%s*\n", i,j, com[i].depend[j]);
	   for (i=0; i<tcnt; i++)
	   for (j=0; j<com[i].ccnt; j++)
	   printf("%d.%s.\n",i, com[i].command[j]);


	   for (i=0; i<p; i++) {
	   printf("`%s`\n", pathname[i]);
	   }
	 */

}


void changeCommand(void) {
	char newCommand[BUFFER_SIZE]= "\0"; // 매크로로 변경되는 command를 저장
	char tmp[BUFFER_SIZE];
	char ch, tmpc;
	int i=0, k=0, c=0, j=0, n=0, l=0, t=0;


	// i: target 수
	for (i=0; i<tcnt; i++) {
		// j: command 수
		for (j=0; j<com[i].ccnt; j++) {
			tmpc = '\0';
			n=0;
			// k: command 한글자씩 읽기
			for (k=0; com[i].command[j][k]!='\0'; k++) {
				ch = com[i].command[j][k];
				if (ch == '$') {
					tmpc = '$';
					continue;
				}

				if (tmpc == '$') {
					// 괄호 빼기
					if (ch != '(' && ch != ')' && ch != '@' && ch != '*') {
						tmp[c] = ch;
						c++;
					}
					else if (ch == ')') {
						tmp[c] = '\0';
						tmpc = '\0';
						c=0;

						// tmp에 들어있는 단어가 mv구조체에 있는 macro에 있는지 확인
						for (l=0; l<mcnt; l++) {
							if (!strcmp(tmp, mv[l].macro)) {	// 있으면 tmp를 value값으로 바꿈
								strcpy(tmp, mv[l].value);
								break;
							}
							else {	// 없으면 에러
								fprintf(stderr, "find not macro\n");
								exit(1);
							}
						}
						// tmp의 단어를 newCommand에 한 글자씩 넣기
						for (l=0; tmp[l]!='\0'; l++) {	
							newCommand[n] = tmp[l];
							n++;
						}
					}
					// 내부 매크로 @일때
					else if (ch == '@') {
						tmp[c+1] = '\0';
						tmpc = '\0';
						c=0;
						strcpy(tmp, com[i].target);	// tmp에 현재 목표 파일 복사
						for (l=0; tmp[l]!='\0'; l++) {
							newCommand[n] = tmp[l];	//newcommand에 한글자씩 저장
							n++;
						}
					}
					// 내부 매크로 '*'일때
					else if (ch == '*') {
						// '.'전 까지 tmp에 저장 
						for (t=0; com[i].target[t] != '.' ; t++) {
							tmp[c] = com[i].target[t];
							c++;
						}
						tmp[c] = '\0';
						tmpc = '\0';
						c=0;
						for (l=0; tmp[l]!='\0'; l++) {
							newCommand[n] =tmp[l];	// newcommand에 한글자씩 저장
							n++;
						}
					}
				}
				else {
					newCommand[n] = ch;
					n++;
				}
			}
			newCommand[n] = '\0';	// 문장의 마지막에 널 넣기
			strcpy(com[i].command[j], newCommand);	// 기존 command를 newcommand로 덮어씌움
		}
	}
}

void exeCommand(int tnum) {
	int i=0, j=0, k=0;

	for (i=0; i<com[i].ccnt; i++) {
		if (!flag_s)	// -s option 이면 command를 출력하지 않음
			printf("%s\n", com[tnum].command[i]);
		system(com[tnum].command[i]);	// command 실행
	}
}

void errorCheck(char *fname) {
	char tmp[BUFFER_SIZE], ch;
	int line=0, fd, i, j, zero=0;

	// file open
	if ((fd = open(fname, O_RDONLY)) < 0) {
		fprintf(stderr, "make: '%s': No such file or directory\n", fname);
		exit(1);
	}


	i=0;
	while(read(fd, &ch, sizeof(char)) > 0) {
		tmp[i] = ch;
		
		if (ch == '\n') {	 // 한 줄의 끝에 왔다, 한 줄이 저장됨
			line++;	// 줄 수 세기
			tmp[i] = '\0';	
			
			for (j=0; j<i; j++) {
				if (tmp[0] == ' ') {	//0번째에 공백이 있으면 zero =1
					zero= 1;
				}
				// 글자가 들어왔을 때
				if (tmp[j] != ' ' && tmp[j] != '\t' && tmp[j] !='\0') {
					// 0번째에 공백이면 에러
					if (zero) {
						fprintf(stderr, "%s:%d: *** missing seperator. Stop.\n", fname, line);
						exit(1);
					}
					// 뒤에 '#'이 있으면 에러
					if (j!=0 && tmp[j] == '#') {
						fprintf(stderr, "%s:%d: *** missing seperator. Stop.\n", fname, line);
						exit(1);
					}
				}
				zero = 0;
			}
			i=-1;
		}
		
		i++;
	}
	close(fd);
}

void targetCheck(void) {
	int check=0;	// 확인하기 위해
	int i, j, k;
	DIR *dp; 
	struct stat statbuf;
	struct dirent* entry;

	if (tcnt == 0) {
		fprintf(stderr, "make: *** No targets. Stop.\n");
		exit(1);
	}
	//	printf("targetCheck()\n");
	for (i=0; i<tcnt; i++) {
		for (j=0; j<com[i].dcnt; j++) {
			check = 0;


			// 타겟에 디펜던시가 있는지 확인
			for (k=0; k<tcnt ; k++) {
				if (!strcmp(com[i].depend[j],com[k].target)) {
					check = 1;
					break;
				}
			}

			// 디펜던시가 디렉토리에 있는지 확인
			if (!check) {
				if ((dp = opendir(".")) == NULL) {	// dir open
					fprintf(stderr, "open directory error\n");
					exit(1);
				}
				while ((entry = readdir(dp)) != NULL) {
					lstat(entry->d_name, &statbuf);
					if (S_ISREG(statbuf.st_mode)) {
						// 파일 찾기
						if (!strcmp(entry->d_name, com[i].depend[j])) {
							check = 1;
							break;
						}
					}
				}
			}

			// 둘다 없으면 에러
			if (!check) {
				fprintf(stderr, "make: *** No rule to make target '%s', needed by '%s'. Stop.\n", com[i].target, com[i].depend[j]);
				exit(1);
			}

		}
	}
}

int dependFind(int tnum) {
	int i, j, check=0;
	DIR *dp;
	struct stat statbuf;
	struct stat tarbuf;
	struct dirent* entry;
	int update=0;

	// bufcnt: tnumbuf에 저장된 개수
	// tnumbuf: tnum을 저장하는 배열
	// tnum: 저장되어있는 target의 번호 com[i].target의 i.


	if (bufcnt == 0) {	// bufcnt가 0이면 tnumbuf 0번째에 저장
		tnumbuf[0] = tnum;
		bufcnt++;
	}
	else {
		for (i=0; i<bufcnt; i++) {	// tnumbuf안에 tnum이 들어왔었는지 확인
			if (tnumbuf[i] == tnum) {	// 들어왔었으면 순환성이므로 리턴해버리기
				fprintf(stderr, "make: Circular %s <- %s dependency dropped.\n", com[tnumbuf[bufcnt-1]].target, com[tnum].target);
				return 1;
			}
		}
		// 없으면 tnumbuf에 tnum넣기
		tnumbuf[bufcnt] = tnum;
		bufcnt++;
	}

	// dependecy가 없으면 커맨드 바로 실행
	if (com[tnum].dcnt == 0) {
		exeCommand(tnum);
		return 1;
	}

	// target에 해당하는 dependency의 개수만큼 
	for (i=0; i<com[tnum].dcnt; i++) {
		check = 0;

		// 타겟에 있는 지 체크
		for (j=0; j<tcnt; j++) {
			if (!strcmp(com[tnum].depend[i], com[j].target)) {
				update = dependFind(j);
				check = 1;
			}
		}

		// 디렉토리에 있는지 체크
		if (!check) {
			if ((dp = opendir(".")) == NULL) {
				fprintf(stderr, "open directory error\n");
				exit(1);
			}
			while ((entry = readdir(dp)) != NULL) {
				lstat(entry->d_name, &statbuf);
				if (S_ISREG(statbuf.st_mode)) {
					if (!strcmp(entry->d_name, com[tnum].depend[i])) {
						check = 1;
						break;
					}
				}
			}
		}

		// 타겟에 없는데 티렉토리에도 없음
		if (!check) {
			fprintf(stderr, "make: %s: No such file or direcotry\n", com[tnum].depend[i]);
			exit(1);
		}
	}


	if (!update){
		// 타켓과 디펜던시 수정시간 비교
		for (i=0; i<com[tnum].dcnt; i++) {

			// target이 파일이 아니면 command 실행
			if (!dirCheck(com[tnum].target)) {
				exeCommand(tnum);
				return 1;
			}
			else {

				// dependency가 파일이 아니면 command 실행
				if (!dirCheck(com[tnum].depend[i])) {
					exeCommand(tnum);
					return 1;
				}
				else {

					// target < dependency
					if (timeCheck(tnum, i) != 0) {
						exeCommand(tnum);
						return 1;
					}
				}
			}
		}
		return 0;
	}
	exeCommand(tnum);
	return 1;	
}

int timeCheck(int tnum, int dnum) {
	struct stat tgbuf;
	struct stat dpbuf;
	struct dirent* entry;
	DIR *dp;
	int check = 0;

	lstat(com[tnum].target, &tgbuf);
	lstat(com[tnum].depend[dnum], &dpbuf);

	if ((dp = opendir(".")) == NULL) {
		fprintf(stderr, "open directory error\n");
		exit(1);
	}

	// 타겟에서 파일이 없는 것 찾기
	while ((entry = readdir(dp)) != NULL) {
		lstat(entry->d_name, &tgbuf);
		if (S_ISREG(tgbuf.st_mode)) {
			if (!strcmp(entry->d_name, com[tnum].target)) {
				check = 1;
				break;
			}
		}
	}

	// 타겟에서 파일이 아닌 것이라면
	if (!check)
		return 2;

	// 디펜던시와 타겟의 시간 비교
	while ((entry = readdir(dp)) != NULL) {
		lstat(entry->d_name, &dpbuf);
		if (S_ISREG(dpbuf.st_mode)) {
			if (!strcmp(entry->d_name, com[tnum].depend[dnum])) {
				if (tgbuf.st_mtime < dpbuf.st_mtime)
					return 1;
				else 
					return 0;
			}
		}
	}
	// 디펜던시에 파일이 아닌 것이 있다면
	return 2;
}
int dirCheck(char *fname) {
	DIR *dp;
	struct stat statbuf;
	struct stat filebuf;
	struct dirent* entry;

	if ((dp = opendir(".")) == NULL) {
		fprintf(stderr, "opendir error\n");
		exit(1);
	}


	lstat(fname, &filebuf);

	// fname이 파일이면 파일과 비교해서 찾기
	if (S_ISREG(filebuf.st_mode)) {
		while ((entry = readdir(dp)) != NULL) {
			lstat(entry->d_name, &statbuf);
			if (S_ISREG(statbuf.st_mode)) {
				if (!strcmp(entry->d_name, fname)) {
					// 있으면 리턴1
					return 1;
				}
			}
		}
		// 없으면 리턴0
		return 0;
	}

	// fname이 디렉터리이면 디렉터리 비교해서 찾기
	else if (S_ISDIR(filebuf.st_mode)) {
		while ((entry = readdir(dp)) != NULL) {
			lstat(entry->d_name, &statbuf);
			if (S_ISDIR(statbuf.st_mode)) {
				if (!strcmp(entry->d_name, fname)) {
					// 있으면 리턴1
					return 1;
				}
			}
		}
		//없으면 리턴0
		return 0;
	}
	else {
		return 0;
	}
}
// include 파일 처리함수
void includef(void) {
	int i;

	// pathname이 비어있지 않다면
	if (pcnt != 0) {
		for (i=0 ; i<pcnt; i++) {
			// pathname에 저장된 파일 파싱하기
			parsing(pathname[i]);
		}
	}
}



