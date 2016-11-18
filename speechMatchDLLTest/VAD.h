/*--------------------------------------------------------------------------------*/
/** Name: define and realize VAD class----Using Eigen Library					 **/
/** Description£º Three model---Pitch estimation¡¢subspectrum Enhancment¡¢VAD    **/
/** Author: Wang jian feng														 **/
/** Date  : 2015.8.8                                                             **/
/** References:A pitch estimation filter robust to high levels of noise (PEFAC)  **/
/*--------------------------------------------------------------------------------*/
#include <iostream>
#include <string>
#include <cmath>
#include <unsupported/Eigen/FFT>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include "afx.h"

using namespace std;
using namespace Eigen;
const double    ENERGYFLOOR = 1.93E-22;
const double	PI	       = 3.141592653589793;
const double	MAX_DBL    = 1.7976931348623158e+308;
const double	MIN_DBL    = 2.2250738585072014e-308;

typedef Matrix<double, 6, 1> Vector6d;
typedef Matrix<double, 1, 6> RowVector6d;

class VAD
{
public:
	VAD(short* buf,int length,double sampleRate,double* &vad_seg_dy,int &vad_num_dy);
	void pitchblockdetect(VectorXd& pv01,VectorXd& pitch,VectorXd& pvblk);
	void vad_snre_round1(VectorXd& dfdata,VectorXi& snre_vad,VectorXd& e);
	void pitchestm(VectorXd& data,VectorXd& pv01,VectorXd& fx);  
	void fxpefac(VectorXd& s,VectorXd& fx,VectorXd& pv);
	void spgrambw(VectorXd& s,VectorXd &t,RowVectorXd &f,MatrixXd &b);
	void enframe(VectorXd &x,RowVectorXd &win,int inc,MatrixXd &f,VectorXd &t);
	void filtbankm_v1(int p,int n,double fs,double fl,double fh,SparseMatrix<double> &x); 
	void filtbankm_v2(int p,int n,double fs,double fl,double fh,SparseMatrix<double> &x,RowVectorXd &cf); 
	void set_fin(RowVectorXd &fin1_, int nf_, double df_,RowVectorXd &fin);
	void stdspectrum(RowVectorXd &cf_,RowVectorXd &b);
	void smooth(RowVectorXd &x,int n);
	void imfilter(MatrixXd &OO_,VectorXd& B,MatrixXd& AF,int flag);
	void findpeaks(RowVectorXd& x,double w,VectorXi& k,VectorXd& v);
	void gaussmixp(MatrixXd& y,MatrixXd& m,MatrixXd& v,Vector6d& w,VectorXd& lp,int flag);
	void get_vector(VectorXd& fpos_,VectorXd &pv_,double inmf_,double flag,VectorXd &a2,int type);
	void specsub_rats_noiseseg_1fn(VectorXd& si,VectorXd& noise_seg,VectorXd &pv01,VectorXd& ss_end);
	void estnoisem_rats_noiseseg(MatrixXd &yf,VectorXd& noise_seg,VectorXd &pv01,double tz,MatrixXd &x);
	void enframe_v2(VectorXd& x,RowVectorXd& win,int inc,MatrixXd &f,VectorXd &t);
	void mhvals(int d,double & m_,double & h_);
	void vad_snre_pv(VectorXd& dfdata,VectorXd& pv01,VectorXd& pvblk,double vadThres,VectorXd& pv_vad);
	void merge(double* &vseg,int vseg_num,double* &vseg_mer,int &vseg_mer_num);
	void split(double* &vseg_mer,int vseg_mer_num,double* &vseg_sp);
	/* friend function  */
	friend void linspace(RowVectorXd &Axis,double MIN_VALUE,double MAX_VALUE);
    friend void linspace_cos(RowVectorXd &Axis, double MIN_VALUE, double MAX_VALUE);
	friend void sort_up(RowVectorXd &ss,RowVectorXd &Record_sort_row,RowVectorXi &Record_sort_num_row);  
	friend void sort_int_up(RowVectorXi &ss,RowVectorXi &Record_sort_row);
	friend int  fix(double r);
	friend void rfft(MatrixXd& x,int n,MatrixXcd& y);   
	friend void irfft(MatrixXcd& y,int n,MatrixXd& x);
	friend void find(RowVectorXd& vec,double limit,int logic,RowVectorXi& pos);
	friend void find_and(RowVectorXd& vec,double low,double high,RowVectorXi& pos);
	friend void Sort_descend(VectorXd& ss,VectorXd& Record_sort_row,VectorXi &Record_sort_num_row);  
	friend void eig(Matrix2d& v_,Matrix2d& uvk_,Matrix2d& dvk_);
	friend double median(VectorXd &Array);
	friend void hamming(int N, RowVectorXd& win);
	friend void sort_up_np(VectorXd ss,VectorXd &Record_sort_row);

private:
	/* fundmental paramter */
	VectorXd	 _data;             /* Record the signal data of wav file */
	double	       _fs;             /* Sample frequency (Hz) */
	int	     _nSamples;             /* Total Sample points (ponits) */
	int	         _flen;             /* frame length points (ponits) */
	int	        _fsh10;             /* frame shift points  (ponits) */
	int	        _nfr10;             /* the total number of frames (numbers) */
};
/* pitchestm data type */
struct Algo_param
{
	double	    fstep;              /* frequency resolution of initial spectrogram (Hz) */
	double	    fmax;               /* maximum frequency of initial spectrogram (Hz) */
	double	    fres;               /* bandwidth of initial spectrogram (Hz) */
	double	    fbanklo;            /* low frequency limit of log filterbank (Hz) */
	int	        mpsmooth;           /* width of smoothing filter for mean power */
	double	    shortut;            /* max utterance length to average power of entire utterance */
	double	    pefact;				/* shape factor in PEFAC filter */
	int	        numopt;				/* number of possible frequencies per frame */	
	RowVector2d flim; 
	RowVector6d w;					/* DP weights */
	double	    tmf;				/* median frequency smoothing interval (s) */
	double	    tinc;				/* default frame increment (s) */
};
/* complex */
struct compx
{
	double	real;
	double	imag;
};
/* subspec */
struct Sub_Algo_param
{
	double	of;             /* overlap factor = (fft length)/(frame increment) */
	double	ti;             /* desired frame increment (16 ms) */
	double	ri;             /* round ni to the nearest power of 2 */
	double	g;              /* subtraction domain: 1=magnitude, 2=power */
	double	e;              /* gain exponent */
	double	am;             /* max oversubtraction factor */
	double	b;              /* noise floor */
	double	al;             /* SNR for maximum a (set to Inf for fixed a) */
	double	ah;             /* SNR for minimum a */
	double	bt;             /* suppress binary masking */
	double	ne;             /* noise estimation: 0=min statistics, 1=MMSE [0] */
	double	mx;             /* no input mixing */
	double	gh;             /* maximum gain */
	char	tf;             /* output the gain time-frequency plane by default */
	double	rf;
};
struct Est_Algo_param
{
	double	taca;           /* smoothing time constant for alpha_c = -tinc/log(0.7) in equ (11) */
	double	tamax;          /* max smoothing time constant in (3) = -tinc/log(0.96) */
	double	taminh;         /* min smoothing time constant (upper limit) in (3) = -tinc/log(0.3) */
	double	tpfall;         /* time constant for P to fall (12) */
	double	tbmax;          /* max smoothing time constant in (20) = -tinc/log(0.8) */
	double	qeqmin;         /* minimum value of Qeq (23) */
	double	qeqmax;         /* max value of Qeq per frame */
	double	av;             /* fudge factor for bc calculation (23 + 13 lines) */
	double	td;             /* time to take minimum over */
	double	nu;             /* number of subwindows */
	double	qith[4];        /* noise slope thresholds in dB/s */
	double	nsmdb[4];
};