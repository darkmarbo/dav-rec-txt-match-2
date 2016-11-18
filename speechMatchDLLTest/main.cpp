#include"VADB.h"
#include <iostream>
#include <string.h>
#include <windows.h>               //使用函数和某些特殊变量



using namespace std;

const int MAX_LEN = 1000;

//typedef void (*DLLFunc)(int,int);  //确定调用函数的形参
typedef int (*DLLFunc_a)(const char *, const char *, double &,double &);
typedef double* (*dll_VADB)(short* ,double ,int ,int& );

//			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
//		fread(&head.sumbytes, 1, sizeof(long), fp);
typedef struct _wavhead
{
	char            riff[4];            //4		“RIFF”;   RIFF标志
	unsigned long   filelong;           //4		0x00 01 06 0A（注意数据存储顺序）;   文件长度
	char            wav[8];             //8		"WAVEfmt "
	unsigned long   t1;					//4		0x12;   sizeof(PCMWAVEFORMAT)         
	short           tag;				//2		1（WAVE_FORMAT_PCM）;  格式类别，1表示为PCM形式的声音数据
	short           channels;			//2		2;  通道数，单声道为1，双声道为2
	unsigned long   samplerate;			//4		44100;  采样频率（每秒样本数）    
	unsigned long   typesps;			//4		0x10B10000;   每秒数据量；其值为通道数×每秒数据位数×每样本的数据位数／8。播放软件利用此值可以估计缓冲区的大小。
	unsigned short  psbytes;            //2		数据块的调整数（按字节算的），其值为通道数×每样本的数据位值／8。播放软件需要一次处理多个该值大小的字节数据，以便将其值用于缓冲区的调整。
	unsigned short  psbits;             //2		每样本的数据位数，表示每个声道中各个样本的数据位数。如果有多个声道，对每个声道而言，样本大小都一样。
	char            data[4];            //4		“data”;   数据标记符
	unsigned long   sumbytes;           //4		0x00 01 05 D8;   语音数据大小
}WAVEHEAD;

// 存储 lab 每行的结构体 
typedef struct _lab
{
	char name; // 语音名字  track1
	double st; // 开始时间  
	double end; // 结束时间 
	std::string cont; // 存储内容
}LAB;

int ReadFileLength(const char *wfile,int* sampleRate);
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow);
/*  p_lab 开辟的需要足够大 len_lab */
int read_lab(const char *file_lab, LAB *p_lab, int len_lab );
int read_wav_head(const char *wfile, WAVEHEAD &head);
int write_wav(const char *file, WAVEHEAD *wav_head, short *buff, int len_buff);
int adjust_pos(double &pos, int flag, double st_end);


int main(int argc, char* argv[])
{
	if(argc < 4)
	{
		printf("usage: %s wav lab out.txt",argv[0]);
		return 0;
	}

	// 加载vad模块
	HINSTANCE hInstLibrary = LoadLibrary(L"VADB.dll"); //加载.dll
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
	
	// 参数变量 
	int ret = 0;
	double time = 0.0;
	double a=1.0;
	double b=0.0;
	int smp_rate_int = 0;  // 采样率
	double smp_rate = 0.0;
	int smp_count = 0;  // 读取语音文件总长度 （多少帧）
	int num = 0;
	int num_all = 0;
	int len_lab = 0;

	LAB *p_lab = new LAB[MAX_LEN];
	WAVEHEAD wav_head;
	
	
	// 存储vad后的结果  [0.01,1.23],[1.50,2.45]
	double *seg_small = NULL; 
	
	char *file_wav = argv[1];
	char *file_lab = argv[2]; // Bears\\lab_ren\\01_Track_01.lab
	char *file_outlog = argv[3];

	printf("%s\t%s\n",file_wav, file_lab);
	FILE *fp_log = fopen(file_outlog,"a+");

	// 从lab名字中提取track名字
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


	// 读取lab文件 
	len_lab = read_lab(file_lab, p_lab, MAX_LEN);
	if (len_lab < 1)
	{
		printf("read lab file err!");
		return 0;
	}

	// 原始的 wav_head  对于每个文件 需要修改 
	//ret = read_wav_head(file_wav, wav_head); 
			
	// 读取采样率和采样点总数 
	smp_count = ReadFileLength(file_wav, &smp_rate_int); 
	smp_rate = double(smp_rate_int);
	printf("smp_rate=%d\tsmp_count:%d\n",smp_rate_int, smp_count);

	// 存储整个语音的buff数据 
	short *buff = new short[smp_count];  
	ret = ReadFile(file_wav, buff, 0, smp_count);
	if (ret < 0)
	{
		printf("readFile err!\n");
		return 0;
	}

	//  lab中每一段 经过vad 修正 
	for (int ii = 0; ii < len_lab; ii++)
	{
		double st = (p_lab + ii)->st;
		double end = (p_lab + ii)->end;
		std::string cont = (p_lab + ii)->cont;
		//fprintf(fp_log, "LOG_lab:%s\t%.4f\t%.4f\t%s\n",
		//	str_lab.c_str(), st, end, cont.c_str());
		//printf("st=%.4f\tend=%.4f\tcont=%s\n",st,end,cont.c_str());

		// vad检测 得到每个小段的 st和end 
		int count_left = int(st*smp_rate);  // 左侧对应采样点
		int count_right = int(end*smp_rate);  // 右侧对应采样点

		// vad函数 buff, smp_rate, smp_count, num
		seg_small = dll_VADB_fun(buff+count_left, smp_rate, count_right-count_left, num);

		// 将num个segment 合并到一个  也就是只选取 st1 和 end_num   
		//printf("seg_num %d\n", num);

		double st_new = st + seg_small[0];
		double end_new = st + seg_small[2*num-1];

		// 端点调整 
		ret = adjust_pos(st_new, 1, st);
		if (ret != 0)
		{
			printf("ERROR:vad processed result err!");
			continue;
		}
		ret = adjust_pos(end_new, 0, end);
		if (ret != 0)
		{
			printf("ERROR:vad processed result err!");
			continue;
		}

		
		//for (int ii = 0; ii < num; ii++)
		//{
			//fprintf(fp_log, "%d\t%.4f\t%.4f\n", ii,
			//	seg_small[2 * ii], seg_small[2 * ii + 1]);
			//fflush(fp_log);
		//}

		// vad检测后的 新 st end  输出 
		fprintf(fp_log, "%s\t%.4f\t%.4f\t%s\n",
			str_lab.c_str(), st_new, end_new, cont.c_str());
		fflush(fp_log);

		/*
		// 同时将该段 语音 输出
		std::string wav_path_out = str_lab;
		char str_d[20] = {0};
		sprintf_s(str_d, "_%.3f", st_new);
		wav_path_out += std::string(str_d);
		sprintf_s(str_d, "_%.3f.wav", end_new);
		wav_path_out += std::string(str_d);

		FILE *fp_temp = NULL;
		if ((fp_temp = fopen(wav_path_out.c_str(), "w")) == NULL)
			return -1;

		
		int count_out = (end_new - st_new)*smp_rate;
		wav_head.sumbytes = count_out*sizeof(short);

		// 从哪个point开始写   写多大块   写多少个 
		fwrite(&wav_head, sizeof(WAVEHEAD), 1, fp_temp);
		fwrite(buff + int(st_new*smp_rate), sizeof(short), count_out, fp_temp);

		if (fp_temp){ fclose(fp_temp); fp_temp = NULL; }
		*/
	}


						

			
	// 释放 与 关闭 
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
	if (p_lab){ delete []p_lab; p_lab = NULL; }

	return 0;

	
}
/*
	flag=1表示st 否则表示end,返回非0失败
*/
int adjust_pos(double &pos,int flag,double st_end)
{
	// 最少留 200ms 不然就用原来的宽度 
	double POS_1 = 0.40;
	double POS_2 = 0.30;
	double POS_3 = 0.20;
	double POS_4 = 0.10;

	if (st_end<0)
	{
		printf("ERROR:input st_end=%.4f err!\n",st_end);
		return -1;
	}

	if (flag == 1) // st
	{
		if (pos - POS_1>st_end )
		{
			pos -= POS_1;
		}
		else if (pos - POS_2>st_end )
		{
			pos -= POS_2;
		}
		else if (pos - POS_3>st_end)
		{
			pos -= POS_3;
		}
		else if (pos - POS_4>st_end)
		{
			pos -= POS_4;
		}
		else
		{
			pos = st_end + 0.01;
		}
	}
	else // end
	{
		if (pos>st_end)
		{
			printf("ERROR:input st_end=%.4f\tpos=%.4f err!\n",st_end,pos);
			return -2;
		}
		if (pos + POS_1< st_end)
		{
			pos += POS_1;
		}
		else if (pos + POS_2<st_end)
		{
			pos += POS_2;
		}
		else if (pos + POS_3<st_end)
		{
			pos += POS_3;
		}
		else if (pos + POS_4<st_end)
		{
			pos += POS_4;
		}
		else
		{
			pos = st_end - 0.01;
		}
	}


	return 0;
}

int read_lab(const char *file_lab, LAB *p_lab, int len_lab )
{
	// 读取的lab个数
	int len_read = 0;
	char chr_line[MAX_LEN] = {0};
	int pos_st = 0;

	FILE *fp_lab = fopen(file_lab, "r");
	if(!fp_lab){
		printf("读取lab=%s文件失败",file_lab);
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

		// 添加到 p_lab 中 
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

// 把short buff 写到 wav中 
/*
	buff ---> ***.wav
	从 buff开始 写len_buff个short 到文件wfile中 
*/
int write_wav(const char *file, WAVEHEAD *wav_head, short *buff, int len_buff)
{
	int ret = 0;
	FILE *fp = NULL;
	if ((fp = fopen(file, "w")) == NULL)
		return -1;

	return ret;
}

/*
	读取语音wav的头  WAVEHEAD 
*/
int read_wav_head(const char *wfile, WAVEHEAD &head)
{
	bool oflag = false;
	FILE *fp = NULL;
	int SAMFREQ = -1;
	int sample_count = 0, channel_num = 0, readflag = 0;
	int numSample = 0;//读数据长度
	try
	{
		//判断声音文件
		if (strstr(wfile, ".wav")) {
			fp = fopen(wfile, "rb");
			if (fp == NULL) {
				return -2;
			}
			oflag = true;
			fseek(fp, 0, SEEK_END);
			sample_count = ftell(fp) - sizeof(WAVEHEAD);
			fseek(fp, 0, SEEK_SET);
			fread(&head, 1, sizeof(WAVEHEAD), fp);
			//data
			if (head.data[0] != 'd'&&head.data[1] != 'a'&&head.data[2] != 't'&&head.data[3] != 'a')
			{
				fclose(fp);
				return -3;
			}
			//RIFF
			if (head.riff[0] != 'R'&&head.riff[1] != 'I'&&head.riff[2] != 'F'&&head.riff[3] != 'F')
			{
				fclose(fp);
				return -3;
			}
			//"WAVEfmt "
			if (head.wav[0] != 'W'&&head.wav[1] != 'A'&&head.wav[2] != 'V'&&head.wav[3] != 'E'&&head.wav[4] != 'f'&&head.wav[5] != 'm'&&head.wav[6] != 't'&&head.wav[7] != ' ')
			{
				fclose(fp);
				return -3;
			}
			//定位数据
			fseek(fp, (long)(head.t1 - 16) - 4, SEEK_CUR);
			fread(&head.sumbytes, 1, sizeof(long), fp);
			//得到字节数
			sample_count = head.sumbytes;
			if (head.samplerate>48000 || head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;
		}
		//得到样本数（n个通道样本数和，且为16bit）
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			return -4;
		}

		fclose(fp);
		oflag = false;
	}
	catch (...)
	{
		if (oflag)
			fclose(fp);

		return -6;
	}

	return 0;
}

// 从bias处开始读取 halfWindow 个short， 如果不够，返回-1。
int ReadFile(const char *wfile, short* allbuf, int bias, int halfWindow)
{
	bool oflag=false;
	FILE *fp=NULL;
	WAVEHEAD head;
	int SAMFREQ=-1;
	int sample_count=0,channel_num=0,readflag=0;
	int numSample = 0;//读数据长度
	try
	{
		//判断声音文件
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
			//定位数据
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//得到字节数
			sample_count=head.sumbytes;
			if(head.samplerate>48000||head.samplerate<0)
			{
				fclose(fp);
				exit(-1);
			}
			SAMFREQ = head.samplerate;
			channel_num = head.channels;
		}
		//得到样本数（n个通道样本数和，且为16bit）
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			return -4;
		}
		// 分配空间读取数据
		// 从bias的开始读取 halfWindow 个short， 如果不够，返回-1。
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
	int numSample = 0;//读数据长度
	try
	{
		//判断声音文件
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
			//定位数据
			fseek(fp,(long)(head.t1-16)-4,SEEK_CUR);
			fread(&head.sumbytes,1,sizeof(long),fp);
			//得到字节数
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
		//得到样本数（n个通道样本数和，且为16bit）
		sample_count /= sizeof(short);
		if (sample_count % channel_num != 0) {
			fclose(fp);
			printf("read channel err!\n");
			return -2;
		}
		/*//分配空间读取数据
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