#include <time.h>
#include <grp.h>
#include <ctype.h>
#include <pwd.h>
#include <utime.h>
#include "head.h"

int flag_s, flag_i, flag_l, flag_n, flag_p, flag_r, flag_d;
int forkN;

void usage() 
{
	// 오류시 해당 명령어의 사용법 출력할 내용
	fprintf(stderr, "ssu_cp error\nusage : in case of file\n\
			\rcp [-i/n][-l][-p][source][target]\n\
			\ror cp [-s][source][target]\n\
			\rin case of directory cp [-i/n][-l][-p][-r][-d][N]\n");
	exit(1);
}

void option(int argc, char *argv[]) 
{
	/* c: 입력받은 옵션 처리하기 위한 변수
	   N: 실행시 입력받은 fork의 개수*/
	int  c, N;

	// 실행 시 입력으로 받은 옵션 처리하기 
	while ((c = getopt(argc, argv, "sSiInNlLpPrRd:D:")) != -1) {
		// 대문자를 소문자로 바꾸기
		c = tolower(c);	
		switch (c) {
			// s옵션 일 때
			case 's':
				if (flag_s) {
					// 같은 옵션이 두번 나올경우 에러처리
					fprintf(stderr, "ssu_cp: option error: -s is repeted\n");
					usage();
					exit(1);
				}
				// 해당 옵션이 입력되었음을 확인하기 위해
				flag_s = 1;
				// 명시적으로 출력해주기
				printf(" s option is on\n");
				break;

			case 'i': 
				if (flag_i) {
					fprintf(stderr, "ssu_cp: option error: -i is repeted\n");
					usage();
					exit(1);
				}
				flag_i = 1;
				printf(" i option is on\n");
				break;

			case 'l':
				if (flag_l) {
					fprintf(stderr, "ssu_cp: option error: -l is repeted\n");
					usage();
					exit(1);
				}
				flag_l = 1;
				printf(" l option is on\n");
				break;

			case 'n': 
				if (flag_n) {
					fprintf(stderr, "ssu_cp: option error: -n is repeted\n");
					usage();
					exit(1);
				}
				flag_n = 1;
				printf(" n option is on\n");
				break;

			case 'p': 
				if (flag_p) {
					fprintf(stderr, "ssu_cp: option error: -p is repeted\n");
					usage();
					exit(1);
				}
				flag_p = 1;
				printf(" p option is on\n");
				break;

			case 'r': 
				if (flag_r) {
					fprintf(stderr, "ssu_cp: option error: -r is repeted\n");
					usage();
					exit(1);
				}
				flag_r = 1;
				printf(" r option is on\n");
				break;

			case 'd':
				if (flag_d) {
					fprintf(stderr, "ssu_cp: option error: -d is repeted\n");
					usage();
					exit(1);
				}
				printf(" d option is on\n");
				// d 다음으로 들어온 N값이 숫자가 아닐 때, N을 주어주지 않았을 때
				if ((forkN = atoi(optarg)) == 0) {
					// 에러처리
					fprintf(stderr, "ssu_cp: the number N must come after d option and N must not be zero\n");
					usage();
					exit(1);
				}
				// N의 값이 1-10까지의 숫자가 아닐경우 
				if (forkN < 1 || forkN > 10) {
					// 에러처리
					fprintf(stderr, "ssu_cp: the number N should range from 1 to 10\n");
					usage();
					exit(1);
				}
				flag_d = 1;
				break;

			default : 
				usage();
				exit(1);
		}
	}
}

void infoCopy(char *source, char *target) {
	struct stat statbuf;
	struct utimbuf time_buf;

	// statbuf에 source파일 정보 저장
	if (stat(source, &statbuf) < 0) {
		perror("infoCopy");
		exit(1);
	}

	// 수정시간을 복사하기 위해 source의 시간 정보 저장
	time_buf.actime = statbuf.st_atime;
	time_buf.modtime = statbuf.st_mtime;

	// target에 수정시간 변경
	if (utime(target, &time_buf) < 0) {
		perror("utime");
		exit(1);
	}

	// target의 권한을 source의 권한 값으로 변경
	if (chmod(target, statbuf.st_mode) < 0) {
		perror("chmod");
		exit(1);
	}

	// target의 소유주, 그룹에 복사
	if (chown(target, statbuf.st_uid, statbuf.st_gid) < 0) {
		perror("chown");
		exit(1);
	}

}

void infoPrint(char *source) {
	struct stat file_info;
	struct tm *at, *mt, *ct;
	struct passwd *u;
	struct group *g;

	// file_info에 source 파일 정보 저장
	if (stat(source, &file_info) < 0) {
		perror("file_info");
		exit(1);
	}

	printf("*******************file info*******************\n");
	printf("파일 이름 : %s\n", source);

	// 초단위의 시간을 tm구조체로 변환
	at = localtime(&file_info.st_atime);
	printf("데이터의 마지막 읽은 시간 : ");
	// 출력형태에 맞게 날짜 및 시간 출력
	printf("%04d.%02d.%02d ", at->tm_year+1900, at->tm_mon+1, at->tm_mday);
	printf("%02d:%02d:%02d\n", at->tm_hour, at->tm_min, at->tm_sec);

	mt = localtime(&file_info.st_mtime);
	printf("데이터의 마지막 수정 시간 : ");
	printf("%04d.%02d.%02d ", mt->tm_year+1900, mt->tm_mon+1, mt->tm_mday);
	printf("%02d:%02d:%02d\n", mt->tm_hour, mt->tm_min, mt->tm_sec);

	ct = localtime(&file_info.st_ctime);
	printf("데이터의 마지막 변경 시간 : ");
	printf("%04d.%02d.%02d ", ct->tm_year+1900, ct->tm_mon+1, ct->tm_mday);
	printf("%02d:%02d:%02d\n", ct->tm_hour, ct->tm_min, ct->tm_sec);

	// 소유주 출력
	u = getpwuid(file_info.st_uid);
	printf("OWNER : %s\n", u->pw_name);

	// 그룹 출력
	g = getgrgid(file_info.st_gid);
	printf("GROUP : %s\n", g->gr_name);

	printf("file size : %ld\n", file_info.st_size);
	return ;
}

