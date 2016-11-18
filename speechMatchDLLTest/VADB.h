/*------------------------------------------------------------  
输入: Buf         (double型数组)
      Length      (Buf长度,也即采样点数)     
	  SampleRate  (采样率)
      Vad_value   (用于保存VAD端点检测结果,大小 Vad_num*2)
	  Vad_num     (用于保存VAD端点数) 
输出: 返回flag = 1,端点检测成功; 返回flag = -1,端点检测失败
---------------------------------------------------------------*/
#ifdef  VAD_EXPORTS
#define VAD_API __declspec(dllexport)
#else
#define VAD_API __declspec(dllimport)
#endif

extern "C" VAD_API double* VADB(short* buf,double SampleRate,int SamplePoints,int& seg_num);





