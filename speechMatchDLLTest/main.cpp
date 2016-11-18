#include"VADB.h"
#include <iostream>
#include <string.h>
#include <windows.h>               //ʹ�ú�����ĳЩ�������



using namespace std;

const int MAX_LEN = 1000;

//typedef void (*DLLFunc)(int,int);  //ȷ�����ú������β�
typedef int (*DLLFunc_a)(const char *, const char *, double &,double &);
typedef double* (*dll_VADB)(short* ,double ,int ,int& );

typedef struct _wavhead
{
	char            riff[4];            //"RIFF"
	unsigned long   filelong;           // +8 = File size
	char            wav[8];             //"WAVEfmt "
	unsigned long   t1;                 
	short           tag;
	short           channels;
	unsigned long   samplerate;         
	unsigned long   typesps;            
	unsigned short  psbytes;            
	unsigned short  psbits;             
	char            data[4];            
	unsigned long   sumbytes;           
}WAVEHEAD;

// �洢 lab ÿ�еĽṹ�� 
typedef struct _lab
{
	char name; // ��������  track1
	double st; // ��ʼʱ��  
	double end; // ����ʱ�� 
	std::string cont; // �洢����
}LAB;

int ReadFileLength(const char *wfile,int* sampleRate);
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow);
/*  p_lab ���ٵ���Ҫ�㹻�� len_lab */
int read_lab(const char *file_lab, LAB *p_lab, int len_lab );


int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("usage: %s wav lab out.txt",argv[0]);
		return 0;
	}
	
	int ret = 0;
	double time = 0.0;
	double a=1.0;
	double b=0.0;
	int smp_rate_int = 0;  // ������
	double smp_rate = 0.0;
	int smp_count = 0;  // ��ȡ�����ļ��ܳ��� ������֡��
	int num = 0;
	int num_all = 0;
	LAB *p_lab = new LAB[MAX_LEN];
	int len_lab = 0;
	
	// �洢vad��Ľ��  [0.01,1.23],[1.50,2.45]
	double *seg_small = NULL; 
	
	char *file_wav = argv[1];
	char *file_lab = argv[2]; // Bears\\lab_ren\\01_Track_01.lab
	char *file_outlog = argv[3];

	printf("%s\t%s\n",file_wav, file_lab);
	FILE *fp_log = fopen(file_outlog,"a+");

	// ��lab��������ȡtrack����
	std::string str_lab = file_lab;
	int pos_lab = str_lab.rfind("\\");
	if (pos_lab<0)
	{
		printf("input lab_path err:%s",str_lab.c_str());
		return -1;
	}
	str_lab = str_lab.substr(pos_lab + 1);
	pos_lab = str_lab.rfind(".lab");
	if (pos_lab<0)
	{
		printf("input lab_path err:%s", str_lab.c_str());
		return -1;
	}
	str_lab = str_lab.substr(0, pos_lab);


	// ��ȡlab�ļ� 
	len_lab = read_lab(file_lab, p_lab, MAX_LEN);
	if (len_lab < 1)
	{
		printf("read lab file err!");
		return 0;
	}

	// ���� p_lab 
	for (int ii = 0; ii < len_lab;ii++)
	{
		double st = (p_lab + ii)->st;
		double end = (p_lab + ii)->end;
		std::string cont = (p_lab + ii)->cont;
		//printf("st=%.4f\tend=%.4f\tcont=%s\n",st,end,cont.c_str());
		fprintf(fp_log, "%s\t%.4f\t%.4f\t%s\n", 
			str_lab.c_str(), st, end, cont.c_str());
		fflush(fp_log);
	}

	HINSTANCE hInstLibrary = LoadLibrary(L"VADB.dll"); //����.dll
	if (hInstLibrary == NULL)
	{
		FreeLibrary(hInstLibrary);
		printf("hInstLibrary == NULL\n");
		return 0;
	}


	dll_VADB dll_VADB_fun;
	dll_VADB_fun = (dll_VADB)GetProcAddress(hInstLibrary, "VADB");
	if (dll_VADB_fun == NULL)
	{
		FreeLibrary(hInstLibrary);
		printf("dllFunc_a == NULL\n");
		return 0;
	}
			
	// ��ȡ�����ʺͲ��������� 
	smp_count = ReadFileLength(file_wav, &smp_rate_int); 
	smp_rate = double(smp_rate_int);
	printf("smp_rate=%d\tsmp_count:%d\n",smp_rate_int, smp_count);

	// �洢����������buff���� 
	short *buff = new short[smp_count];  
	ret = ReadFile(file_wav, buff, 0, smp_count);
	if (ret < 0)
	{
		printf("readFile err!\n");
		return 0;
	}

	seg_small = dll_VADB_fun(buff, smp_rate, smp_count, num);
				
	// ������   
	printf("seg_num %d\n", num);								
	for (int ii = 0; ii < num; ii++)
	{
		fprintf(fp_log, "%d\t%.4f\t%.4f\n", ii,
		seg_small[2 * ii], seg_small[2 * ii + 1]);
		fflush(fp_log);
	}
						

			
	// �ͷ� �� �ر� 
	if (hInstLibrary)
	{
		FreeLibrary(hInstLibrary); 
		hInstLibrary= NULL;
	}
	if (buff)
	{ 
		delete buff; buff = NULL; 
	}

	if (fp_log){ fclose(fp_log); fp_log = NULL; }
			
	return 0;

	
}


int read_lab(const char *file_lab, LAB *p_lab, int len_lab )
{
	// ��ȡ��lab����
	int len_read = 0;
	char chr_line[MAX_LEN] = {0};
	int pos_st = 0;

	FILE *fp_lab = fopen(file_lab, "r");
	if(!fp_lab){
		printf("��ȡlab=%s�ļ�ʧ��",file_lab);
		return len_read;
	}

	while(fgets(chr_line, MAX_LEN, fp_lab))
	{
		//printf("line=%s",chr_line);
		int ii = 0;
		// 2.122540	3.842540	by sarah courtauld
		std::string str_line = chr_line;
		if (str_line.substr(str_line.length() - 1).compare("\n") == 0)
		{
			str_line = str_line.substr(0, str_line.length() - 1);
		}

		int pos_find = str_line.find("\t");
		if (pos_find < 0)
		{ 
			printf("lab line format err:%s", chr_line); 
			return 0; 
		}
		// 2.122540	
		std::string str_st = str_line.substr(0, pos_find);
		
		
		// 3.842540	by sarah courtauld
		str_line = str_line.substr(pos_find+1);  
		pos_find = str_line.find("\t");
		if (pos_find < 0)
		{ 
			printf("lab line format err:%s", chr_line); 
			return 0; 
		}
		// 3.842540
		std::string str_end = str_line.substr(0, pos_find);
		

		// by sarah courtauld
		str_line = str_line.substr(pos_find + 1);

		if (str_line.compare("#") == 0 || str_line.compare("#\n") == 0 ||
			str_line.length()<1)
		{
			//printf("content=%s\ttoo small!\n",str_line.c_str());
			continue;
		}

		// ��ӵ� p_lab �� 
		p_lab->st = atof(str_st.c_str());
		p_lab->end = atof(str_end.c_str());
		p_lab->cont = str_line.c_str();

		p_lab += 1;
		len_read += 1;

		if (len_read > len_lab - 1)
		{ 
			printf("input len_lab is too small !\n"); 
			return 0; 
		}

	}

	return len_read;

}

// ��bias����ʼ��ȡ halfWindow ��short�� �������������-1��
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow)
{
	bool oflag=false;
	FILE *fp=NULL;
	WAVEHEAD head;
	int SAMFREQ=-1;
	int sample_count=0,channel_num=0,readflag=0;
	int numSample = 0;//�����ݳ���
	try
	{
		//�ж������ļ�
		if (strstr(wfile, ".wav")) {
			fp=fopen(wfile, "rb");
			if (fp == NULL) {
				return -2;
			}
			oflag=true;
			fseek(fp,0,SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp,0,SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if(head.data[0]!='d'&&head.data[1]!='a'&&head.data[2]!='t'&&head.data[3]!='a')
			{
				fclose(fp);
				return -3;
			}
			//RIFF
			if(head.riff[0]!='R'&&head.riff[1]!='I'&&head.riff[2]!='F'&&head.riff[3]!='F')
			{
				fclose(fp);
				return -3;
			}
			//"WAVEfmt "
			if(head.wav[0]!='W'&&head.wav[1]!='A'&&head.wav[2]!='V'&&head.wav[3]!='E'&&head.wav[4]!='f'&&head.wav[5]!='m'&&head.wav[6]!='t'&&head.wav[7]!=' ')
			{
				fclose(fp);
				return -3;
			}
			//��λ����
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//�õ��ֽ���
			sample_count=head.sumbytes;
			if(head.samplerate>48000||head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;
		}
		//�õ���������n��ͨ���������ͣ���Ϊ16bit��
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			return -4;
		}
		// ����ռ��ȡ����
		// ��bias�Ŀ�ʼ��ȡ halfWindow ��short�� �������������-1��
		printf("bias=%d\tsample_count=%d\thalfWindow=%d\n",bias, sample_count,halfWindow);
		if (bias + halfWindow <= sample_count)
		{
			numSample = halfWindow;
		}
		else
		{
			return -5;
		}
		//allbuf = (short*)malloc(numSample * sizeof(short));
		fseek(fp, bias*sizeof(short), SEEK_CUR);
		fread(allbuf, sizeof(short), numSample,fp);
		
		fclose(fp);
		oflag=false;
	}
	catch(...)
	{
		if(oflag)
			fclose(fp);

		if(allbuf)free(allbuf);
		allbuf=NULL;
		return -6;

	}
	return 0;
}


int ReadFileLength(const char *wfile,int* sampleRate)
{
	bool oflag=false;
	FILE *fp=NULL;
	WAVEHEAD head;
	int SAMFREQ=-1;
	int sample_count=0,channel_num=0,readflag=0;
	int numSample = 0;//�����ݳ���
	try
	{
		//�ж������ļ�
		if (strstr(wfile, ".wav")) {
			fp=fopen(wfile, "rb");
			if (fp == NULL) {
				printf("read %s err!\n", wfile);
				return -1;
			}
			//printf("open file ok!\n");

			oflag=true;
			fseek(fp,0,SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp,0,SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if(head.data[0]!='d'&&head.data[1]!='a'&&head.data[2]!='t'&&head.data[3]!='a')
			{
				fclose(fp);
				printf("read data err!\n");
				return -1;
			}
			//RIFF
			if(head.riff[0]!='R'&&head.riff[1]!='I'&&head.riff[2]!='F'&&head.riff[3]!='F')
			{
				fclose(fp);
				printf("read RIFF err!\n");
				return -1;
			}
			//"WAVEfmt "
			if(head.wav[0]!='W'&&head.wav[1]!='A'&&head.wav[2]!='V'&&head.wav[3]!='E'&&head.wav[4]!='f'&&head.wav[5]!='m'&&head.wav[6]!='t'&&head.wav[7]!=' ')
			{
				fclose(fp);
				printf("read WAVEfmt err!\n");
				return -1;
			}
			//��λ����
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//�õ��ֽ���
			sample_count=head.sumbytes;
			if(head.samplerate>48000||head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;

			*sampleRate = SAMFREQ;
		}
		//�õ���������n��ͨ���������ͣ���Ϊ16bit��
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			printf("read channel err!\n");
			return -2;
		}
		/*//����ռ��ȡ����
		if (bias+MAX<sample_count)
		{
			numSample = MAX;
		}
		else
		{
			numSample = sample_count-bias;
		}
		allbuf = (short*)malloc(numSample * sizeof(short));
		fread(allbuf, sizeof(short), numSample,fp+bias);
		fclose(fp);
		oflag=false;*/

		fclose(fp);
		return sample_count;
	}
	catch(...)
	{
		if(oflag)
			fclose(fp);

		/*if(allbuf)free(allbuf);
		allbuf=NULL;*/
		return -1;

	}

	fclose(fp);
	return 0;
}