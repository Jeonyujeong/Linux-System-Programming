#include "head.h"

extern int flag_s, flag_i, flag_l, flag_n, flag_p, flag_r, flag_d;
extern int forkN;

int main(int argc, char *argv[])
{
	/*	filedir: isfile의 리턴값을 저장해서 파일과 디렉터리를 구분한다.
		source: 입력받은 source의 값 저장
		target: 입력받은 target의 값 저장
	 */
	int filedir;
	char source[PATH_MAX];
	char target[PATH_MAX];

	// option 처리
	option(argc, argv);

	// file 과 directory를 제대로 입력하지 않았을 때 에러처리
	if (argc != optind+2) {
		fprintf(stderr, "ssu_cp: source and target are not clear.\n");
		usage();
		exit(1);
	}
	
	// 입력받은 파일/디렉터리 이름 source, target에 각각 저장
	strcpy(source, argv[optind]);
	strcpy(target, argv[optind+1]);

	// source와 target이 같으면 오류처리
	if (!strcmp(source, target)) {
		fprintf(stderr, "ssu_cp: %s: File must not be same.\n", source);
		usage();
		exit(1);
	}

	// target와 src 명시출력
	printf("target : %s\nsrc : %s\n", target, source);

	// option s단독 사용 아닐시 에러처리
	if (flag_s) {
		if (flag_i | flag_n | flag_l | flag_p) {
			fprintf(stderr, "ssu_cp: -s option must be used alone.\n");
			usage();
			exit(1);
		}
	}

	// option p가 사용되었다면 정보 출력
	if (flag_p)
		infoPrint(source);

	// file과 dir 구분하기 위해 file = 0, dir = 1
	filedir = isfile(source);

	// file
	if (filedir == 1) {
		// file 인데 디렉터리 옵션 사용시 에러처리
		if (flag_r || flag_d) {
			fprintf(stderr, "ssu_cp: -r or -d should be used in directory\n");
			usage();
		}
		// option s가 사용되었다면 symlink 
		if (flag_s) 
			symcopy(source, target);
		// file copy
		else
			filecopy(source, target);
	}

	// directory
	else if (filedir == 0) {
		// -r or -d option이 하나라도 없으면 에러처리
		if (!flag_r && !flag_d){
			fprintf(stderr, "ssu_cp: -r or -d must be used in directory\n");
			usage();
		}
		// -r 과 -d option이 같이 쓰이면 에러처리
		if (flag_r && flag_d) {
			fprintf(stderr, "ssu_cp: Do not used together option -r & -d\n");
			usage();
			exit(1);
		}
		// -s option이 있을 경우 에러처리
		if (flag_s) {
			fprintf(stderr, "ssu_cp: -s option is available in the file\n");
			usage();
		}
		// -r option 이라면 dir copy수행
		if (flag_r) 
			dircopy(source, target);
		// -d option 이라면 dir fork수행 
		else if (flag_d) 
			dirfork(source, target, forkN);
	}

	// source에 해당하는 file/directroy가 없으면 에러처리
	else {
		fprintf(stderr, "ssu_cp: %s: No such file or directory\n", source);
		usage();
		exit(1);
	}
}

