import sys
import os
import shutil

if len(sys.argv)<3:
	print("usage: dir_in dir_out"%(sys.argv[0]));
	sys.exit(0);
	
dir_in=sys.argv[1];
dir_out=sys.argv[2];

if not os.path.isdir(dir_in):
	print("ERROR:%s not exist!"%(dir_in));
	sys.exit(0);

shutil.rmtree(dir_out);
os.mkdir(dir_out);
	
list_story=os.listdir(dir_in);
for story in list_story:
	dir_story = "%s\\%s"%(dir_in,story);
	
	dir_lab="%s\\lab_ren"%(dir_story);
	dir_txt="%s\\txt"%(dir_story);
	dir_wav="%s\\wav_16"%(dir_story);
	
	if not os.path.isdir(dir_wav):
		print("dir_wav:%s not eixst!"%(dir_wav));
	list_wav=os.listdir(dir_wav);
	
	### dir_lab 
	if not os.path.isdir(dir_lab):
		print("dir_lab:%s not exist!"%(dir_lab));
		continue;
	list_lab=os.listdir(dir_lab);
	
	for wav_name in list_wav:
		wav_path = "%s\\%s"%(dir_wav,wav_name);
		#print("wav:%s"%(wav_path));
		ddd=wav_name[-4:];
		if ddd != ".wav":
			print("Warning:%s not is .wav!"%(ddd));
			continue;
		file_wav = wav_name[:-4];
		lab_path = "%s\\%s.lab"%(dir_lab,file_wav)
		if not os.path.isfile(lab_path):
			print("ERROR:%s not exist");
			continue;
		out_path="%s\\%s.out.txt"%(dir_out,story);	
		#### os.syste
		cmd="vad_test.exe  %s %s %s "%(wav_path,lab_path,out_path);
		os.system(cmd);
		


