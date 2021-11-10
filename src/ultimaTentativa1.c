#include <stdio.h>
#include <math.h>
#include "ultimaTentativa1.h"

/***********************************************/
/*   Default methods for membership functions  */
/***********************************************/
/*
static double _defaultMFcompute_smeq(MembershipFunction mf, double x) {
 double y,mu,degree=0;
 for(y=mf.max; y>=x ; y-=mf.step)
  if((mu = mf.compute_eq(mf,y))>degree) degree=mu;
 return degree;
}

static double _defaultMFcompute_greq(MembershipFunction mf, double x) {
 double y,mu,degree=0;
 for(y=mf.min; y<=x ; y+=mf.step)
   if((mu = mf.compute_eq(mf,y))>degree) degree=mu;
 return degree;
}

static double _defaultMFcenter(MembershipFunction mf) {
 return 0;
}

static double _defaultMFbasis(MembershipFunction mf) {
 return 0;
}

/***********************************************/
/* Common functions for defuzzification methods*/
/**********************************************

static double compute(FuzzyNumber fn, double x) {
 double dom,imp;
 int i;
 if(fn.length == 0) return 0;
 imp = fn.op.imp(fn.degree[0],fn.conc[0].compute_eq(fn.conc[0],x));
 dom = imp;
 for(i=1; i<fn.length; i++) {
  imp = fn.op.imp(fn.degree[i],fn.conc[i].compute_eq(fn.conc[i],x));
  dom = fn.op.also(dom,imp);
 }
 return dom;
}

static double center(MembershipFunction mf) {
 return mf.center(mf);
}

static double basis(MembershipFunction mf) {
 return mf.basis(mf);
}

static double param(MembershipFunction mf,int i) {
 return mf.param[i];
}

/***********************************************/
/*  Common functions to create fuzzy numbers   */
/**********************************************

static double MF_xfl_singleton_equal(MembershipFunction _mf,double x);

static FuzzyNumber createCrispNumber(double value) {
 FuzzyNumber fn;
 fn.crisp = 1;
 fn.crispvalue = value;
 fn.length = 0;
 fn.degree = NULL;
 fn.conc = NULL;
 fn.inputlength = 0;
 fn.input = NULL;
 return fn;
}

static FuzzyNumber createFuzzyNumber(int length, int inputlength,
                                     double*input, OperatorSet op) {
 int i;
 FuzzyNumber fn;
 fn.crisp = 0;
 fn.crispvalue = 0;
 fn.length = length;
 fn.degree = (double *) malloc(length*sizeof(double));
 fn.conc = (MembershipFunction *) malloc(length*sizeof(MembershipFunction));
 for(i=0; i<length; i++) fn.degree[i] = 0;
 fn.inputlength = inputlength;
 fn.input = input;
 fn.op = op;
 return fn;
}

static int isDiscreteFuzzyNumber(FuzzyNumber fn) {
 int i;
 if(fn.crisp) return 0;
 for(i=0; i<fn.length; i++)
  if(fn.conc[i].compute_eq != MF_xfl_singleton_equal)
   return 0;
 return 1;
}

/***********************************************/
/*  Functions to compute single propositions   */
/**********************************************

static double _isEqual(MembershipFunction mf, FuzzyNumber fn) {
 int i;
 double x,mu1,mu2,minmu,degree=0;
 if(fn.crisp) return mf.compute_eq(mf,fn.crispvalue);
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = mf.compute_eq(mf,fn.conc[i].param[0]);
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = mf.compute_eq(mf,x);
   mu2 = compute(fn,x);
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isGreaterOrEqual(MembershipFunction mf, FuzzyNumber fn) {
 int i;
 double x,mu1,mu2,minmu,degree=0,greq=0;
 if(fn.crisp) return mf.compute_greq(mf,fn.crispvalue);
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = mf.compute_greq(mf,fn.conc[i].param[0]);
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = mf.compute_eq(mf,x);
   if( mu2>greq ) greq = mu2;
   if( mu1<greq ) minmu = mu1; else minmu = greq;
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isSmallerOrEqual(MembershipFunction mf, FuzzyNumber fn) {
 int i;
 double x,mu1,mu2,minmu,degree=0,smeq=0;
 if(fn.crisp) return mf.compute_smeq(mf,fn.crispvalue);
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = mf.compute_smeq(mf,fn.conc[i].param[0]);
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.max; x>=mf.min; x-=mf.step){
   mu1 = compute(fn,x);
   mu2 = mf.compute_eq(mf,x);
   if( mu2>smeq ) smeq = mu2;
   if( mu1<smeq ) minmu = mu1; else minmu = smeq;
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isGreater(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,gr,degree=0,smeq=0;
 if(fn.crisp) return op.Not(mf.compute_smeq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.Not(mf.compute_smeq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.max; x>=mf.min; x-=mf.step){
   mu1 = compute(fn,x);
   mu2 = mf.compute_eq(mf,x);
   if( mu2>smeq ) smeq = mu2;
   gr = op.Not(smeq);
   minmu = ( mu1<gr ? mu1 : gr);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isSmaller(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,sm,degree=0,greq=0;
 if(fn.crisp) return op.Not(mf.compute_greq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.Not(mf.compute_greq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = mf.compute_eq(mf,x);
   if( mu2>greq ) greq = mu2;
   sm = op.Not(greq);
   minmu = ( mu1<sm ? mu1 : sm);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isNotEqual(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,degree=0;
 if(fn.crisp) return op.Not(mf.compute_eq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.Not(mf.compute_eq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = op.Not(mf.compute_eq(mf,x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isApproxEqual(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,degree=0;
 if(fn.crisp) return op.moreorless(mf.compute_eq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.moreorless(mf.compute_eq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = op.moreorless(mf.compute_eq(mf,x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isVeryEqual(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,degree=0;
 if(fn.crisp) return op.very(mf.compute_eq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.very(mf.compute_eq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = op.very(mf.compute_eq(mf,x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}

static double _isSlightlyEqual(MembershipFunction mf, FuzzyNumber fn, OperatorSet op) {
 int i;
 double x,mu1,mu2,minmu,degree=0;
 if(fn.crisp) return op.slightly(mf.compute_eq(mf,fn.crispvalue));
 if( isDiscreteFuzzyNumber(fn) ) {
  for(i=0; i<fn.length; i++){
   mu1 = op.slightly(mf.compute_eq(mf,fn.conc[i].param[0]));
   minmu = (mu1<fn.degree[i] ? mu1 : fn.degree[i]);
   if( degree<minmu ) degree = minmu;
  }
 }
 else {
  for(x=mf.min; x<=mf.max; x+=mf.step){
   mu1 = compute(fn,x);
   mu2 = op.slightly(mf.compute_eq(mf,x));
   minmu = (mu1<mu2 ? mu1 : mu2);
   if( degree<minmu ) degree = minmu;
  }
 }
 return degree;
}


/***************************************/
/*  MembershipFunction MF_xfl_trapezoid  */
/**************************************
static double MF_xfl_trapezoid_equal(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<a || x>d? 0: (x<b? (x-a)/(b-a) : (x<c?1 : (d-x)/(d-c)))); 
}
static double MF_xfl_trapezoid_greq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<a? 0 : (x>b? 1 : (x-a)/(b-a) )); 
}
static double MF_xfl_trapezoid_smeq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (x<c? 1 : (x>d? 0 : (d-x)/(d-c) )); 
}
static double MF_xfl_trapezoid_center(MembershipFunction _mf) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (b+c)/2; 
}
static double MF_xfl_trapezoid_basis(MembershipFunction _mf) {
 double a = _mf.param[0];
 double b = _mf.param[1];
 double c = _mf.param[2];
 double d = _mf.param[3];
  return (d-a); 
}
static MembershipFunction createMF_xfl_trapezoid( double min, double max, double step, double *param,int length) {
 int i;
 MembershipFunction _mf;
 _mf.min = min;
 _mf.max = max;
 _mf.step = step;
 _mf.param = (double*) malloc(length*sizeof(double));
 for(i=0;i<length;i++) _mf.param[i] = param[i];
 _mf.compute_eq = MF_xfl_trapezoid_equal;
 _mf.compute_greq = MF_xfl_trapezoid_greq;
 _mf.compute_smeq = MF_xfl_trapezoid_smeq;
 _mf.center = MF_xfl_trapezoid_center;
 _mf.basis = MF_xfl_trapezoid_basis;
 return _mf;
}

/***************************************/
/*  MembershipFunction MF_xfl_singleton  */
/**************************************
static double MF_xfl_singleton_equal(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
  return (x==a? 1 : 0); 
}
static double MF_xfl_singleton_greq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
  return (x>=a? 1 : 0); 
}
static double MF_xfl_singleton_smeq(MembershipFunction _mf,double x) {
 double a = _mf.param[0];
  return (x<=a? 1 : 0); 
}
static double MF_xfl_singleton_center(MembershipFunction _mf) {
 double a = _mf.param[0];
  return a; 
}
static MembershipFunction createMF_xfl_singleton( double min, double max, double step, double *param,int length) {
 int i;
 MembershipFunction _mf;
 _mf.min = min;
 _mf.max = max;
 _mf.step = step;
 _mf.param = (double*) malloc(length*sizeof(double));
 for(i=0;i<length;i++) _mf.param[i] = param[i];
 _mf.compute_eq = MF_xfl_singleton_equal;
 _mf.compute_greq = MF_xfl_singleton_greq;
 _mf.compute_smeq = MF_xfl_singleton_smeq;
 _mf.center = MF_xfl_singleton_center;
 _mf.basis = _defaultMFbasis;
 return _mf;
}
/***************************************/
/*  Operatorset OP_operadores */
/**************************************

static double OP_operadores_And(double a, double b) {
  return (a<b? a : b); 
}

static double OP_operadores_Or(double a, double b) {
  return (a>b? a : b); 
}

static double OP_operadores_Also(double a, double b) {
  return (a>b? a : b); 
}

static double OP_operadores_Imp(double a, double b) {
  return (a<b? a : b); 
}

static double OP_operadores_Not(double a) {
  return 1-a; 
}

static double OP_operadores_Very(double a) {
 double w = 2.0;
  return pow(a,w); 
}

static double OP_operadores_MoreOrLess(double a) {
 double w = 0.5;
  return pow(a,w); 
}

static double OP_operadores_Slightly(double a) {
  return 4*a*(1-a); 
}

static double OP_operadores_Defuz(FuzzyNumber mf) {
 double min = mf.conc[0].min;
 double max = mf.conc[0].max;
 double step = mf.conc[0].step;
   double x, m, num=0, denom=0;
   for(x=min; x<=max; x+=step) {
    m = compute(mf,x);
    num += x*m;
    denom += m;
   }
   if(denom==0) return (min+max)/2;
   return num/denom;
}

static OperatorSet createOP_operadores() {
 OperatorSet op;
 op.And = OP_operadores_And;
 op.Or = OP_operadores_Or;
 op.also = OP_operadores_Also;
 op.imp = OP_operadores_Imp;
 op.Not = OP_operadores_Not;
 op.very = OP_operadores_Very;
 op.moreorless = OP_operadores_MoreOrLess;
 op.slightly = OP_operadores_Slightly;
 op.defuz = OP_operadores_Defuz;
 return op;
}


/***************************************/
/*  Type TP_areaJogador */
/**************************************

static TP_areaJogador createTP_areaJogador() {
 TP_areaJogador tp;
 double min = 0.0;
 double max = 105.0;
 double step = 0.4117647058823529;
 double _p_mtPerto[4] = { -9.545454545454545,0.0,14.49090909090909,21.636363636363633 };
 double _p_perto[4] = { 19.09090909090909,21.636363636363633,35.72727272727273,43.272727272727266 };
 double _p_medio[4] = { 39.72727272727273,44.272727272727266,66.36363636363636,76.9090909090909 };
 double _p_longe[4] = { 73.36363636363636,76.9090909090909,105.0,114.54545454545453 };
 tp.mtPerto = createMF_xfl_trapezoid(min,max,step,_p_mtPerto,4);
 tp.perto = createMF_xfl_trapezoid(min,max,step,_p_perto,4);
 tp.medio = createMF_xfl_trapezoid(min,max,step,_p_medio,4);
 tp.longe = createMF_xfl_trapezoid(min,max,step,_p_longe,4);
 return tp;
}

/***************************************/
/*  Type TP_posXgoleiro */
/**************************************

static TP_posXgoleiro createTP_posXgoleiro() {
 TP_posXgoleiro tp;
 double min = 0.0;
 double max = 45.0;
 double step = 0.17647058823529413;
 double _p_recuado[4] = { -4.090909090909091,0.0,3.481818181818182,5.472727272727273 };
 double _p_avancadoArea[4] = { 4.581818181818182,5.672727272727273,9.454545454545453,15.545454545454547 };
 double _p_medio[4] = { 13.454545454545453,15.545454545454547,23.72727272727273,29.81818181818182 };
 double _p_avancado[4] = { 27.72727272727273,29.81818181818182,45.0,49.09090909090909 };
 tp.recuado = createMF_xfl_trapezoid(min,max,step,_p_recuado,4);
 tp.avancadoArea = createMF_xfl_trapezoid(min,max,step,_p_avancadoArea,4);
 tp.medio = createMF_xfl_trapezoid(min,max,step,_p_medio,4);
 tp.avancado = createMF_xfl_trapezoid(min,max,step,_p_avancado,4);
 return tp;
}

/***************************************/
/*  Type TP_possChute */
/**************************************

static TP_possChute createTP_possChute() {
 TP_possChute tp;
 double min = 0.0;
 double max = 10.0;
 double step = 0.0392156862745098;
 double _p_baixa[4] = { -1.25,0.0,2.5,3.25 };
 double _p_media[4] = { 2.9,3.75,6.25,7.0 };
 double _p_alta[4] = { 6.7,7.5,10.0,11.25 };
 tp.baixa = createMF_xfl_trapezoid(min,max,step,_p_baixa,4);
 tp.media = createMF_xfl_trapezoid(min,max,step,_p_media,4);
 tp.alta = createMF_xfl_trapezoid(min,max,step,_p_alta,4);
 return tp;
}

/***************************************/
/*  Type TP_areaHori */
/***************************************

static TP_areaHori createTP_areaHori() {
 TP_areaHori tp;
 double min = 0.0;
 double max = 68.0;
 double step = 0.26666666666666666;
 double _p_direita[4] = { -8.5,0.0,17.0,25.5 };
 double _p_meio[4] = { 17.0,25.5,42.5,51.0 };
 double _p_esquerda[4] = { 42.5,51.0,68.0,76.5 };
 tp.direita = createMF_xfl_trapezoid(min,max,step,_p_direita,4);
 tp.meio = createMF_xfl_trapezoid(min,max,step,_p_meio,4);
 tp.esquerda = createMF_xfl_trapezoid(min,max,step,_p_esquerda,4);
 return tp;
}

/***************************************/
/*  Type TP_qtJogadores */
/**************************************

static TP_qtJogadores createTP_qtJogadores() {
 TP_qtJogadores tp;
 double min = 0.0;
 double max = 10.0;
 double step = 0.0392156862745098;
 double _p_vantagem[4] = { -0.9090909090909091,0.0,2.5,3.5 };
 double _p_empate[4] = { 3.0,3.5,5.5,6.0 };
 double _p_desvantagem[4] = { 5.5,6.2,7.5,7.9 };
 double _p_largaDes[4] = { 7.5,8.181818181818182,10.0,10.909090909090908 };
 tp.vantagem = createMF_xfl_trapezoid(min,max,step,_p_vantagem,4);
 tp.empate = createMF_xfl_trapezoid(min,max,step,_p_empate,4);
 tp.desvantagem = createMF_xfl_trapezoid(min,max,step,_p_desvantagem,4);
 tp.largaDes = createMF_xfl_trapezoid(min,max,step,_p_largaDes,4);
 return tp;
}

/***************************************/
/*  Rulebase RL_regras */
/**************************************

static void RL_regras(FuzzyNumber possibilidades, FuzzyNumber areaCampo, FuzzyNumber *respostaGoleiro) {
 OperatorSet _op = createOP_operadores();
 double _rl, _output;
 int _i_respostaGoleiro=0;
 TP_possChute _t_possibilidades = createTP_possChute();
 TP_areaJogador _t_areaCampo = createTP_areaJogador();
 TP_posXgoleiro _t_respostaGoleiro = createTP_posXgoleiro();
 double *_input = (double*) malloc(2*sizeof(double));
 _input[0] = possibilidades.crispvalue;
 _input[1] = areaCampo.crispvalue;
 *respostaGoleiro = createFuzzyNumber(12,2,_input,_op);
 _rl = _op.And(_isEqual(_t_possibilidades.baixa,possibilidades),_isEqual(_t_areaCampo.mtPerto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.recuado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.baixa,possibilidades),_isEqual(_t_areaCampo.perto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.recuado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.baixa,possibilidades),_isEqual(_t_areaCampo.medio,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.baixa,possibilidades),_isEqual(_t_areaCampo.longe,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.media,possibilidades),_isEqual(_t_areaCampo.mtPerto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.recuado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.media,possibilidades),_isEqual(_t_areaCampo.perto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.recuado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.media,possibilidades),_isEqual(_t_areaCampo.medio,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.medio;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.media,possibilidades),_isEqual(_t_areaCampo.longe,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.alta,possibilidades),_isEqual(_t_areaCampo.mtPerto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancadoArea;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.alta,possibilidades),_isEqual(_t_areaCampo.perto,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.recuado;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.alta,possibilidades),_isEqual(_t_areaCampo.medio,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancadoArea;
 _i_respostaGoleiro++;
 _rl = _op.And(_isEqual(_t_possibilidades.alta,possibilidades),_isEqual(_t_areaCampo.longe,areaCampo));
 respostaGoleiro->degree[_i_respostaGoleiro] = _rl;
 respostaGoleiro->conc[_i_respostaGoleiro] =  _t_respostaGoleiro.avancado;
 _i_respostaGoleiro++;
 _output = _op.defuz(*respostaGoleiro);
 free(respostaGoleiro->degree);
 free(respostaGoleiro->conc);
 *respostaGoleiro = createCrispNumber(_output);
}

/***************************************/
/*  Rulebase RL_definePoss */
/**************************************

 void RL_definePoss(FuzzyNumber areaCampo, FuzzyNumber numJogadores, FuzzyNumber *poss) {
 OperatorSet _op = createOP_operadores();
 double _rl, _output;
 int _i_poss=0;
 TP_areaJogador _t_areaCampo = createTP_areaJogador();
 TP_qtJogadores _t_numJogadores = createTP_qtJogadores();
 TP_possChute _t_poss = createTP_possChute();
 double *_input = (double*) malloc(2*sizeof(double));
 _input[0] = areaCampo.crispvalue;
 _input[1] = numJogadores.crispvalue;
 *poss = createFuzzyNumber(16,2,_input,_op);
 _rl = _op.And(_isEqual(_t_areaCampo.longe,areaCampo),_isEqual(_t_numJogadores.vantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.longe,areaCampo),_isEqual(_t_numJogadores.empate,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.longe,areaCampo),_isEqual(_t_numJogadores.desvantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.longe,areaCampo),_isEqual(_t_numJogadores.largaDes,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.medio,areaCampo),_isEqual(_t_numJogadores.vantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.medio,areaCampo),_isEqual(_t_numJogadores.empate,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.medio,areaCampo),_isEqual(_t_numJogadores.desvantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.medio,areaCampo),_isEqual(_t_numJogadores.largaDes,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.perto,areaCampo),_isEqual(_t_numJogadores.vantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.alta;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.perto,areaCampo),_isEqual(_t_numJogadores.empate,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.perto,areaCampo),_isEqual(_t_numJogadores.desvantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.perto,areaCampo),_isEqual(_t_numJogadores.largaDes,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.baixa;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.mtPerto,areaCampo),_isEqual(_t_numJogadores.vantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.alta;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.mtPerto,areaCampo),_isEqual(_t_numJogadores.empate,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.alta;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.mtPerto,areaCampo),_isEqual(_t_numJogadores.desvantagem,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _rl = _op.And(_isEqual(_t_areaCampo.mtPerto,areaCampo),_isEqual(_t_numJogadores.largaDes,numJogadores));
 poss->degree[_i_poss] = _rl;
 poss->conc[_i_poss] =  _t_poss.media;
 _i_poss++;
 _output = _op.defuz(*poss);
 free(poss->degree);
 free(poss->conc);
 *poss = createCrispNumber(_output);
}

/***************************************/
/*          Inference Engine           */
/**************************************

void ultimaTentativa1InferenceEngine(double _d_possibilidades, double _d_areaCampo, double _d_areaAtual, double _d_numJogadores, double *_d_respostaGoleiro, double *_d_poss) {
 FuzzyNumber possibilidades = createCrispNumber(_d_possibilidades);
 FuzzyNumber areaCampo = createCrispNumber(_d_areaCampo);
 FuzzyNumber areaAtual = createCrispNumber(_d_areaAtual);
 FuzzyNumber numJogadores = createCrispNumber(_d_numJogadores);
 FuzzyNumber respostaGoleiro, poss;
 RL_definePoss(areaAtual, numJogadores, &poss);
 RL_regras(possibilidades, areaCampo, &respostaGoleiro);
 *_d_respostaGoleiro = (respostaGoleiro.crisp? respostaGoleiro.crispvalue :respostaGoleiro.op.defuz(respostaGoleiro));
 *_d_poss = (poss.crisp? poss.crispvalue :poss.op.defuz(poss));
}

*/