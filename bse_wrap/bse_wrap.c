#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "bse_wrap.h"

/* calculate metallicity constants */
void bse_zcnsts(double *z, double *zpars)
{
  zcnsts_(z, zpars);
}

/* evolve a single star */
void bse_evolv1(int *kw, double *mass, double *mt, double *r, double *lum,
		double *mc, double *rc, double *menv, double *renv, double *ospin,
		double *epoch, double *tms, double *tphys, double *tphysf, 
		double *dtp, double *z, double *zpars, double *vs)
{
  /* must null out vs, since SSE/BSE is not designed to return it and hence doesn't null it out */
  vs[0] = 0.0;
  vs[1] = 0.0;
  vs[2] = 0.0;
  evolv1_(kw, mass, mt, r, lum, mc, rc, menv, renv, ospin,
	  epoch, tms, tphys, tphysf, dtp, z, zpars, vs);
}

/* evolve a single star safely: in some cases, a merger can have non self-consistent properties, */
/* leading to crazy things like NaN radii---this is the easiest way to get around that problem */
void bse_evolv1_safely(int *kw, double *mass, double *mt, double *r, double *lum,
		       double *mc, double *rc, double *menv, double *renv, double *ospin,
		       double *epoch, double *tms, double *tphys, double *tphysf, 
		       double *dtp, double *z, double *zpars, double *vs)
{
  int mykw, kattempt=-1;
  double mymass, mymt, myr, mylum, mymc, myrc, mymenv, myrenv, myospin, myepoch;
  double mytms, mytphys, mytphysf, mydtp, myvs[3], tphystried;

  do {
    kattempt++;
    mykw = *kw;
    mymass = *mass;
    mymt = *mt;
    myr = *r;
    mylum = *lum;
    mymc = *mc;
    myrc = *rc;
    mymenv = *menv;
    myrenv = *renv;
    myospin = *ospin;
    myepoch = *epoch;
    mytms = *tms;
    mytphys = BSE_WRAP_MAX(*tphys - ((float) kattempt) * pow(1.2, kattempt), 0.0);
    tphystried = mytphys;
    mytphysf = *tphysf;
    mydtp = *dtp;
    bse_evolv1(&mykw, &mymass, &mymt, &myr, &mylum, &mymc, &myrc, &mymenv, &myrenv, &myospin,
	       &myepoch, &mytms, &mytphys, &mytphysf, &mydtp, z, zpars, myvs);
  } while ((isnan(myr) || mymc < 0.0 || mymt < 0.0 || mymass < 0.0 || mylum < 0.0) && tphystried > 0.0);

  if (tphystried == 0.0) {
    fprintf(stderr, "bse_evolv1_safely(): Artifical age reduction failed.\n");
    exit(1);
  } else if (kattempt > 1) {
    fprintf(stderr, "bse_evolv1_safely(): Artifical age reduction succeeded.\n");
    fprintf(stderr, "bse_evolv1_safely(): kattempt=%d age_reduction=%g Myr.\n", kattempt, *tphys-tphystried);
  }

  *kw = mykw;
  *mass = mymass;
  *mt = mymt;
  *r = myr;
  *lum = mylum;
  *mc = mymc;
  *rc = myrc;
  *menv = mymenv;
  *renv = myrenv;
  *ospin = myospin;
  *epoch = myepoch;
  *tms = mytms;
  *tphys = mytphys;
  *tphysf = mytphysf;
  *dtp = mydtp;
  vs[0] = myvs[0];
  vs[1] = myvs[1];
  vs[2] = myvs[2];
}

/* evolve a binary */
void bse_evolv2(int *kstar, double *mass0, double *mass, double *rad, double *lum, 
		double *massc, double *radc, double *menv, double *renv, double *ospin,
		double *epoch, double *tms, double *tphys, double *tphysf, double *dtp,
		double *z, double *zpars, double *tb, double *ecc, double *vs)
{
  /* must null out vs, since SSE/BSE is not designed to return it and hence doesn't null it out */
  vs[0] = 0.0;
  vs[1] = 0.0;
  vs[2] = 0.0;
  evolv2_(kstar, mass0, mass, rad, lum, massc, radc, menv, renv, ospin,
	  epoch, tms, tphys, tphysf, dtp, z, zpars, tb, ecc, vs);
}

/* evolve a binary star safely: in some cases, a merger can have non self-consistent properties, */
/* leading to crazy things like NaN radii---this is the easiest way to get around that problem */
void bse_evolv2_safely(int *kstar, double *mass0, double *mass, double *rad, double *lum, 
		       double *massc, double *radc, double *menv, double *renv, double *ospin,
		       double *epoch, double *tms, double *tphys, double *tphysf, double *dtp,
		       double *z, double *zpars, double *tb, double *ecc, double *vs)
{
  int mykstar[2], kattempt=-1, j;
  double mymass0[2], mymass[2], myrad[2], mylum[2], mymassc[2], myradc[2], mymenv[2], myrenv[2], myospin[2], myepoch[2];
  double mytms[2], mytphys, mytphysf, mydtp, tphystried, mytb, myecc, myvs[3];

  do {
    kattempt++;
    for (j=0; j<2; j++) {
      mykstar[j] = kstar[j];
      mymass0[j] = mass0[j];
      mymass[j] = mass[j];
      myrad[j] = rad[j];
      mylum[j] = lum[j];
      mymassc[j] = massc[j];
      myradc[j] = radc[j];
      mymenv[j] = menv[j];
      myrenv[j] = renv[j];
      myospin[j] = ospin[j];
      myepoch[j] = epoch[j];
      mytms[j] = tms[j];
    }
    mytphys = BSE_WRAP_MAX(*tphys - ((float) kattempt) * pow(1.2, kattempt), 0.0);
    tphystried = mytphys;
    mytphysf = *tphysf;
    mydtp = *dtp;
    mytb = *tb;
    myecc = *ecc;
    bse_evolv2(mykstar, mymass0, mymass, myrad, mylum, mymassc, myradc, mymenv, myrenv, myospin,
	       myepoch, mytms, &mytphys, &mytphysf, &mydtp, z, zpars, &mytb, &myecc, myvs);
  } while ((isnan(myrad[0]) || mymassc[0] < 0.0 || mymass[0] < 0.0 || mymass0[0] < 0.0 || mylum[0] < 0.0 || 
	    isnan(myrad[1]) || mymassc[1] < 0.0 || mymass[1] < 0.0 || mymass0[1] < 0.0 || mylum[1] < 0.0) && tphystried > 0.0);
  
  if (tphystried == 0.0) {
    fprintf(stderr, "bse_evolv2_safely(): Artifical age reduction failed.\n");
    exit(1);
  } else if (kattempt > 1) {
    fprintf(stderr, "bse_evolv2_safely(): Artifical age reduction succeeded.\n");
    fprintf(stderr, "bse_evolv2_safely(): kattempt=%d age_reduction=%g Myr.\n", kattempt, *tphys-tphystried);
  }
  
  for (j=0; j<2; j++) {
      kstar[j] = mykstar[j];
      mass0[j] = mymass0[j];
      mass[j] = mymass[j];
      rad[j] = myrad[j];
      lum[j] = mylum[j];
      massc[j] = mymassc[j];
      radc[j] = myradc[j];
      menv[j] = mymenv[j];
      renv[j] = myrenv[j];
      ospin[j] = myospin[j];
      epoch[j] = myepoch[j];
      tms[j] = mytms[j];
    }
  *tphys = mytphys;
  *tphysf = mytphysf;
  *dtp = mydtp;
  *tb = mytb;
  *ecc = myecc;
  vs[0] = myvs[0];
  vs[1] = myvs[1];
  vs[2] = myvs[2];
}

/* set collision matrix */
void bse_instar(void)
{
  instar_();
}

/* setters */
void bse_set_idum(int idum) { value3_.idum = idum; }
void bse_set_neta(double neta) { value1_.neta = neta; }
void bse_set_bwind(double bwind) { value1_.bwind = bwind; }
void bse_set_hewind(double hewind) { value1_.hewind = hewind; }
void bse_set_sigma(double sigma) { value4_.sigma = sigma; }
void bse_set_ifflag(int ifflag) { flags_.ifflag = ifflag; }
void bse_set_wdflag(int wdflag) { flags_.wdflag = wdflag; }
void bse_set_bhflag(int bhflag) { value4_.bhflag = bhflag; }
void bse_set_nsflag(int nsflag) { flags_.nsflag = nsflag; }
void bse_set_mxns(double mxns) { value1_.mxns = mxns; }
void bse_set_pts1(double pts1) { points_.pts1 = pts1; }
void bse_set_pts2(double pts2) { points_.pts2 = pts2; }
void bse_set_pts3(double pts3) { points_.pts3 = pts3; }
void bse_set_alpha1(double alpha1) { value2_.alpha1 = alpha1; }
void bse_set_lambda(double lambda) { value2_.lambda = lambda; }
void bse_set_ceflag(int ceflag) { flags_.ceflag = ceflag; }
void bse_set_tflag(int tflag) { flags_.tflag = tflag; }
void bse_set_beta(double beta) { value5_.beta = beta; }
void bse_set_xi(double xi) { value5_.xi = xi; }
void bse_set_acc2(double acc2) { value5_.acc2 = acc2; }
void bse_set_epsnov(double epsnov) { value5_.epsnov = epsnov; }
void bse_set_eddfac(double eddfac) { value5_.eddfac = eddfac; }
void bse_set_gamma(double gamma) { value5_.gamma = gamma; }

/* getters */
/* note the index flip and decrement so the matrices are accessed
   as they would be in fortran */
float bse_get_spp(int i, int j) { return(single_.spp[j-1][i-1]); }
float bse_get_scm(int i, int j) { return(single_.scm[j-1][i-1]); }
float bse_get_bpp(int i, int j) { return(binary_.bpp[j-1][i-1]); }
float bse_get_bcm(int i, int j) { return(binary_.bcm[j-1][i-1]); }

char *bse_get_sselabel(int kw)
{
  if (kw == 0) {
    return("Low Mass MS Star");
  } else if (kw == 1) {
    return("Main sequence Star");
  } else if (kw == 2) {
    return("Hertzsprung Gap");
  } else if (kw == 3) {
    return("Giant Branch");
  } else if (kw == 4) {
    return("Core Helium Burning");
  } else if (kw == 5) {
    return("First AGB");
  } else if (kw == 6) {
    return("Second AGB");
  } else if (kw == 7) {
    return("Naked Helium MS");
  } else if (kw == 8) {
    return("Naked Helium HG");
  } else if (kw == 9) {
    return("Naked Helium GB");
  } else if (kw == 10) {
    return("Helium WD");
  } else if (kw == 11) {
    return("Carbon/Oxygen WD");
  } else if (kw == 12) {
    return("Oxygen/Neon WD");
  } else if (kw == 13) {
    return("Neutron Star");
  } else if (kw == 14) {
    return("Black Hole");
  } else if (kw == 15) {
    return("Massless Supernova");
  } else {
    return("Unkown Stellar Type!");
  }
}

char *bse_get_bselabel(int kw)
{
  if (kw == 1) {
    return("INITIAL");
  } else if (kw == 2) {
    return("KW CHNGE");
  } else if (kw == 3) {
    return("BEG RCHE");
  } else if (kw == 4) {
    return("END RCHE");
  } else if (kw == 5) {
    return("CONTACT");
  } else if (kw == 6) {
    return("COELESCE");
  } else if (kw == 7) {
    return("COMENV");
  } else if (kw == 8) {
    return("GNTAGE");
  } else if (kw == 9) {
    return("NO REMNT");
  } else if (kw == 10) {
    return("MAX TIME");
  } else if (kw == 11) {
    return("DISRUPT");
  } else if (kw == 12) {
    return("BEG SYMB");
  } else if (kw == 13) {
    return("END SYMB");
  } else if (kw == 14) {
    return("BEG BSS");
  } else {
    return("Unkown Stellar Type!");
  }
}

/* kick speed from distribution, taken directly from BSE code */
/* may change startype in certain cases */
double bse_kick_speed(int *startype)
{
  int k;
  double u1, u2, s, theta, vk2, vk, v[4]; /* yes, v is supposed to be 4-D */

  for (k=1; k<=2; k++) {
    u1 = ran3_(&(value3_.idum));
    u2 = ran3_(&(value3_.idum));
    s = value4_.sigma * sqrt(-2.0*log(1.0-u1));
    theta = 2.0 * M_PI * u2;
    v[2*k-1-1] = s*cos(theta);
    v[2*k-1] = s*sin(theta);
  }
  vk2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
  vk = sqrt(vk2);

  if (((*startype) == 14 && value4_.bhflag == 0) || (*startype) < 0) {
    vk2 = 0.0;
    vk = 0.0;
    if ((*startype) < 0) {
      (*startype) = 13;
    }
  }

  return(vk);
}
