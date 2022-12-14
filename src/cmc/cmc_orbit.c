/* -*- linux-c -*- */
/* vi: set filetype=c.doxygen: */
/* vim: set shiftwidth=2 */

/* use commandline option -DUSE_THREADS to turn on threading */
/*#define USE_THREADS*/
#ifdef USE_THREADS
#include <pthread.h>
#define NUM_THREADS	2
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <time.h>
#include "cmc.h"
#include "cmc_vars.h"

/**
* @brief ?
*
* @param index ?
* @param E energy
* @param J angular momentum
*
* @return ?
*/
long get_positive_Q_index(long index, double E, double J) {
  long ktemp, fevals;
  double Qtemp;
	int g_si = get_global_idx(index);
    long newpartstart= mpiEnd-mpiBegin+1;

  ktemp= g_si;
  fevals=1;

  if (index > newpartstart) {
    ktemp = FindZero_r(0, clus.N_MAX, star[index].rnew) + 1;
  };

  Qtemp = function_q(g_si, star_r[ktemp], star_phi[ktemp], E, J);

  /* check for nearly circular orbit and do linear search */
  if (Qtemp <= 0.0) {
    ktemp = -1;
    do {
      ktemp++; fevals++;
      Qtemp = function_q(g_si, star_r[ktemp], star_phi[ktemp], E, J);
    } while (Qtemp <= 0.0 && ktemp <= clus.N_MAX);		
    if (ktemp >= clus.N_MAX) {
      dprintf("There is no positive Q index for index=%li, star.id=%li!\n", index, 
          star[index].id);
      return(-1);
    }
  };
  return(ktemp);
};

/**
* @brief
* Calculates Q at star position kmid and follows N_Q_TRACE Q-evaluations
* symmetrically to both sides of kmid. If there are no positive Q-values and
* Q has been decreasing monotonously a circular orbit is returned (-1). If
* there was a positive Q-value its index k is returned. Otherwise the next
* N_Q_TRACE points are evaluated. If the loop ends without result -2 is returned
*
* @param index ?
* @param kmid ?
* @param E ?
* @param J ?
* @param kmin ?
* @param kmax ?
*
* @return ?
*/
long symmetric_positive_Q_search(long index, long kmid, double E, double J, long kmin, long kmax) {
  long ktemp, sign, i, fevals;
  long sym_min, sym_max, sym_len;
  double Qtemp= -1.;
  int left= 0, right= 0, n_trace= 0; /* counters */
  int dec_mono_left= 0, dec_mono_right= 0;
  double Qlast_left, Qlast_right; 

  /* calculate the index region where we can search symmetrically */
  if (kmax- kmid> kmid) {
    sym_min= kmin;
    sym_max= 2*kmid;
    sym_len= kmid-1;
  } else {
    sym_min= kmid- (kmax- kmid);
    sym_max= kmax;
    sym_len= (kmax- kmid);
  };

  fevals= 0;
  /* symmetric search loop */
  sign=1; 
  n_trace= MIN(2*N_Q_TRACE, 2*sym_len);
  printf("sym_len=%li, sym_min=%li, sym_max=%li, n_trace=%i\n", sym_len, sym_min, 
      sym_max, n_trace);
  for (i=2; i<2*sym_len; i++) {
    sign*= -1; fevals++;
    ktemp= kmid+ sign*i/2;
    n_trace--;
    /* checking for monotonicity */
    if (sign==-1) {
      left++;
      if (left==1) {
        Qtemp= function_q(index, star[ktemp].r, star[ktemp].phi, E, J);
        dec_mono_left=0;
      } else {
        Qlast_left= Qtemp;
        Qtemp= function_q(index, star[ktemp].r, star[ktemp].phi, E, J);
        dec_mono_left= dec_mono_left && Qlast_left> Qtemp;
      };
    };
    if (sign==1) {
      right++;
      if (right==1) {
        Qtemp= function_q(index, star[ktemp].r, star[ktemp].phi, E, J);
        dec_mono_right=0;
      } else {
        Qlast_right= Qtemp;
        Qtemp= function_q(index, star[ktemp].r, star[ktemp].phi, E, J);
        dec_mono_right= dec_mono_right && Qlast_right> Qtemp;
      };
    };

    /* we stop if Qtemp is positive */
    if (Qtemp> 0.) break;

    /* we stop if Q was monotonously decreasing for the last N_Q_TRACE points */
    if (dec_mono_left && dec_mono_right && n_trace==0) {
      ktemp= -1;
      break;
    };

    /* reset counters if we tested N_Q_TRACE points to either side */
    if (n_trace== 0) {
      left= 0; right= 0;
      n_trace= MIN(2*N_Q_TRACE, 2*sym_len-i);
    };

    /* Signal failure when the interval ends are reached. 
     * (If the loop continues ktemp is calculated again) */
    ktemp= -2;
  };

  //printf("s--> index=%li, kmid=%li, fevals=%li\n", index, kmid, fevals);

  return (ktemp);
};


/**
* @brief ?
*
* @param index ?
* @param E ?
* @param J ?
* @param orbit_old ?
*
* @return ?
*/
long get_positive_Q_index_new_J(long index, double E, double J, orbit_rs_t orbit_old) {
  long ktemp, fevals, kmid, i, kmin, kmax;
  long double Qtemp;

  ktemp= index;
  fevals=1;
  kmax= orbit_old.kmax;
  kmin= orbit_old.kmin;

  Qtemp = function_q(index, star[ktemp].r, star[ktemp].phi, E, J);
  if (Qtemp<=0.0) {
    kmid= (orbit_old.kmax+orbit_old.kmin)/2.;
    Qtemp= function_q(index, star[kmid].r, star[kmid].phi, E, J);
    if (Qtemp<= 0.0) {
      ktemp= symmetric_positive_Q_search(index, kmid, E, J, 0, clus.N_MAX+1);
      dprintf("sym search index=%li, kmid=%li, ktemp=%li\n", index, kmid, ktemp);
      if (ktemp==-2) {
        if (clus.N_MAX+ 1- kmid> kmid) {
          dprintf("kmax-kmid>kmid\n");
          kmax= clus.N_MAX+ 1;
          kmin= 2*kmid;
        } else {
          dprintf("else\n");
          kmax= kmid- (clus.N_MAX+ 1- kmid);
          kmin= 0;
        };
        ktemp= -1;
        for (i=kmin; i<= kmax; i++) {
          fevals++;
          Qtemp= function_q(index, star[i].r, star[i].phi, E, J);
          if (Qtemp>0.) {
            ktemp= i;
            break;
          };
        };
      };
    } else {
      ktemp= kmid;
    };
  };

  if (ktemp==-1) {
    dprintf("There is no positive Q index for index=%li, star.id=%li!\n", 
        index, star[index].id);
    ktemp= get_positive_Q_index(index, E, J);
    if (ktemp!=-1) {
      eprintf("KATASTROPHE!! Got index=%li, ktemp=%li\n", index, ktemp);
      exit_cleanly(-1, __FUNCTION__);
    };
    return(-1);
  };

  //printf(">>> index=%li, fevals=%li, ktemp=%li, q=%Lg\n", index, fevals, ktemp, Qtemp);
  return(ktemp);
};


/**
* @brief ?
*
* @param index ?
* @param E ?
* @param J ?
* @param old_r ?
* @param orbit ?
*
* @return ?
*/
struct star_coords get_position(long index, double E, double J, double old_r, orbit_rs_t orbit) {
  struct star_coords new_pos;
  double vr, r, pot, drds, Q;
  double rmin, rmax, X, dQdr_min, dQdr_max, g1, g2, g0, F, s0;
  long k;

  /* Set E, J and index as they will not change */
  new_pos.E= E;
  new_pos.J= J;
  new_pos.field_index= index;

  E+= MPI_PHI_S(old_r, index);
  /* remove massless stars (boundary stars or stellar evolution victims) */
  /* note that energy lost due to stellar evolution is subtracted
     at the time of mass loss in DoStellarEvolution */
  if (star_m[index] < ZERO) {
    eprintf("Star has zero mass! Should have been already removed! Bail out.");
    exit_cleanly(-1, __FUNCTION__);
  }

  /* remove unbound stars */
  if (E >= 0.0) {
    eprintf("Star is unbound! Should have been already removed! Bail out.");
    exit_cleanly(-1, __FUNCTION__);
  }

  /* skip the rest if the star is on a nearly circular orbit */
  if (!orbit.circular_flag) {
    rmin = orbit.rp;
    rmax = orbit.ra;
    dQdr_min = orbit.dQdrp;
    dQdr_max = orbit.dQdra;

    g1 = sqrt(3.0 * (rmax - rmin) / dQdr_min);	/* g(-1) */
    g2 = sqrt(-3.0 * (rmax - rmin) / dQdr_max);	/* g(+1) */

    F = 1.2 * MAX(g1, g2);

    for (k = 1; k <= N_TRY; k++) {
		X = rng_t113_dbl_new(curr_st); 
      //X = rng_t113_dbl();

      s0 = 2.0 * X - 1.0;	 /* random -1 < s0 < 1 */

		g0 = F * rng_t113_dbl_new(curr_st); 
      //g0 = F * rng_t113_dbl(); /* random  0 < g0 < F */

      r = 0.5 * (rmin + rmax) + 0.25 * (rmax - rmin) * (3.0 * s0 - s0 * s0 * s0);

      pot = potential(r) + MPI_PHI_S(r, index);

      drds = 0.25 * (rmax - rmin) * (3.0 - 3.0 * s0 * s0);
      Q = 2.0 * E - 2.0 * pot - J * J / r / r;

      if (Q >= 0.0) {
        vr = sqrt(Q);
      } else {
        dprintf("circular orbit: vr^2<0: setting vr=0: si=%ld r=%g rmin=%g rmax=%g vr^2=%g X=%g E=%g J=%g\n",
            index, r, rmin, rmax, Q, X, E, J);
        if (isnan(Q)) {
          eprintf("fatal error: Q=vr^2==nan!\n");
          exit_cleanly(-1, __FUNCTION__);
        }
        vr = 0;
      }
      if (g0 < 1.0 / vr * drds)	/* if g0 < g(s0) then success! */
        break;
    }
    if (k == N_TRY + 1) {
      eprintf("N_TRY exceeded\n");
      exit_cleanly(-1, __FUNCTION__);
    };

    /* pick random sign for v_r */
    //if (rng_t113_dbl() < 0.5)
		if(rng_t113_dbl_new(st) < 0.5)

      vr = -vr;

    new_pos.r  = r;
    new_pos.vr = vr;
    new_pos.vt = J/r;
    new_pos.pot= pot- MPI_PHI_S(r, index);
  } else { /* for a circular orbit */
    new_pos.r = orbit.rp;
    new_pos.vr= 0.;
    new_pos.vt= J/orbit.rp;
    new_pos.pot= potential(orbit.rp);
  };

  /* The star interval of the new position is marked as
   * "undefined" (-1) and gets only calculated during sub-steps */
  new_pos.index= -1;

  return(new_pos);
};

/**
* @brief ?
*
* @param actual_rmin ?
* @param rmin ?
* @param inside_sqrt ?
* @param k ?
* @param ismin ?
*/
void debug_msg_orbit_new_inside_sqrt(double actual_rmin, double rmin, double inside_sqrt,
    long k, int ismin) {
  double rk, rk1;
  char *label;

  label= ismin? "min": "max";

  rk= star_r[k];
  rk1= star_r[k+1];

  dprintf("The sqrt in the expression for rmin has a negative argument!");
  dprintf("It is, therefore, set to zero.\n");
  dprintf("r%s_old= %g r%s_new= %g r%s_sqrt= %g\n", label, actual_rmin, label, rmin, label, inside_sqrt);
  dprintf("star[k%s+1].r= %g star[k%s].r= %g star[k%s+1].r-star[k%s].r= %g\n",
      label, rk1, label, rk, label, label, rk1-rk);
  dprintf("star[k%s+1].r-r%s= %g, r%s-star[k%s].r= %g\n", label, label,
      rk1-rmin, label, label, rmin-rk);
}


/**
 * @brief Print supplementary information related to MPI parallelization.
 *
 * @param msg Message context string
 * @param proc Current processor
 * @param g_si Global particle index
 * @param si  Local particle index
 * @param mpiBegin Start of memory partition in global address space
 * @param mpiEnd  End of memory partition in global address space
 */
void debug_msg_supp_mpi_info(char *msg, int proc, long g_si, long si,
    int mpiBegin, int mpiEnd) {
    dprintf("%s: mpiBegin=%i, mpiEnd=%i, global index=%li, local index=%li, myid=%i\n",
        msg, mpiBegin, mpiEnd, g_si, si, myid);
}

/**
* @brief ?
*
* @param si ?
* @param rmin ?
* @param rmin_new ?
* @param k ?
* @param rk ?
* @param rk1 ?
* @param ismin ?
* @param E ?
* @param J ?
*/
void debug_msg_orbit_new_outside_interval(long si, double rmin, double rmin_new,
    long k, double rk, double rk1, int ismin, double E, double J) {
  char *label;

  label= ismin? "min": "max";

  dprintf("r%s=%g outside of interval (%g,%g)@%li, using bisection to get root.\n",
      label, rmin, rk, rk1, k);
  dprintf("r%s outside: si=%ld, E=%g, J=%g\n", label, si, E, J);

  dprintf("new r%s=%g, r%s in interval? %s\n", label, rmin_new, label,
      (rmin_new>=rk)&&(rmin_new<=rk1)? "Yes": "No");
}

/**
* @brief finds out new orbit of star, primarily peri and apo- center distances
*
* @param index star index
* @param E energy
* @param J angular momentum
*
* @return orbit structure containing properties of new orbit
*/
orbit_rs_t calc_orbit_new(long index, double E, double J) {
  orbit_rs_t orbit_rs;
  long ktemp, kmin, kmax;
  double rmin, rmax, rkmin, rk1min, rkmax, rk1max;
  double a, b, dQdr_min, dQdr_max;
  double dQdr_min_num, dQdr_max_num;
  int circular;
	int g_si = get_global_idx(index);

  circular=0;

  ktemp= get_positive_Q_index(index, E, J);
  if (ktemp<=-1) {
    dprintf("Did not find a single positive Q value! index= %d\n", g_si);
    dprintf("circular orbit found: index=%d sr=%g svr=%g svt=%g J=%g E=%g\n",
        g_si, star_r[g_si], star[index].vr, star[index].vt, star[index].J, star[index].E);

    orbit_rs.rp = star_r[g_si];
    orbit_rs.ra = star_r[g_si];
    orbit_rs.kmin = g_si;
    orbit_rs.kmax = g_si;
    orbit_rs.dQdrp = 0.0;
    orbit_rs.dQdra = 0.0;
    orbit_rs.circular_flag = 1;
    return (orbit_rs);
  }

  /* calculate new kmin */
  kmin= find_zero_Q(g_si, 0, ktemp, E, J);

  /* calculate new kmax */
  kmax= find_zero_Q(g_si, ktemp, clus.N_MAX +1, E, J);

  rkmin= star_r[kmin];
  rk1min= star_r[kmin+1];
  rkmax= star_r[kmax];
  rk1max= star_r[kmax+1];

  /* calculate rmin and rmax */
  if (!circular) {
    double inside_sqrt, actual_rmin, actual_rmax; 

    /* First we try Henon's method. If it fails we use bisection. */
    set_a_b(g_si, kmin, &a, &b);
    inside_sqrt= a * a - 2.0 * J * J * (b - E);

    if (inside_sqrt<0.0) {
      rmin = -1.0 *J * J / a ;
      actual_rmin = J * J / (-a + sqrt(a * a - 2.0 * J * J * (b - E)));

      debug_msg_orbit_new_inside_sqrt(actual_rmin, rmin, inside_sqrt, kmin, 1);

      if ((rmin<rkmin) || (rmin>rk1min)) {
        double rmin_new;

        rmin_new= find_root_vr(g_si, kmin, E, J);

        debug_msg_orbit_new_outside_interval(index, rmin, rmin_new, kmin,
            rkmin, rk1min, 1, E, J);
        debug_msg_supp_mpi_info("rmin outside", myid, g_si, index, mpiBegin, mpiEnd);
        rmin= rmin_new;
      }
    } else {
      rmin = J * J / (-a + sqrt(inside_sqrt));
    }

    dQdr_min = 2.0 * J * J / (rmin * rmin * rmin) + 2.0 * a / (rmin * rmin);
    dQdr_min_num = (function_Q(index, kmin+1, E, J)-function_Q(index, kmin, E, J))/(star_r[kmin+1]-star_r[kmin]);

    set_a_b(g_si, kmax, &a, &b);
    inside_sqrt= a * a - 2.0 * J * J * (b - E);

    if (inside_sqrt<0.0){
      rmax = -a / (2.0 * (b - E));
      actual_rmax = (-a + sqrt(a * a - 2.0 * J * J * (b - E))) / (2.0 * (b - E));

      debug_msg_orbit_new_inside_sqrt(actual_rmax, rmax, inside_sqrt, kmax, 0);

      if ((rmax<rkmax) || (rmax>rk1max)) {
        double rmax_new;

        rmax_new= find_root_vr(g_si, kmax, E, J);

        debug_msg_orbit_new_outside_interval(index, rmax, rmax_new, kmax,
            rkmax, rk1max, 0, E, J);
        debug_msg_supp_mpi_info("rmax outside", myid, g_si, index, mpiBegin, mpiEnd);
        rmax= rmax_new;
      }
    } else {
      rmax = (-a + sqrt(inside_sqrt)) / (2.0 * (b - E));
    }
    
    dQdr_max = 2.0 * J * J / (rmax * rmax * rmax) + 2.0 * a / (rmax * rmax);
    dQdr_max_num = (function_Q(index, kmax+1, E, J)- function_Q(index, kmax, E, J))/(star_r[kmax+1]-star_r[kmax]);
  };
 
  /* another case of a circular orbit */
  if (!circular && ((rmin > rmax)||(dQdr_min<=0.)||(dQdr_max>=0.))) {
    eprintf("circular orbit found!\n");
    eprintf("Check Here: rmin=%g>rmax=%g: kmin=%ld kmax=%ld si=%ld r=%g vr=%g vt=%g J=%g E=%g Q(kmin)=%g Q(kmax)=%g dQdr_min=%g dQdr_min=%g dQdr_min_num=%g dQdr_max_num=%g\n",
	rmin, rmax, kmin, kmax, index, star_r[index], star[index].vr, star[index].vt, star[index].J, star[index].E,
	function_Q(index, kmin, star[index].E, star[index].J), function_Q(index, kmax, star[index].E, star[index].J),
	dQdr_min,dQdr_max,dQdr_min_num,dQdr_max_num);
    circular= 1;
  }

  /* Set the return variables. */
  if (circular) {
    dprintf("circular orbit found: index=%ld sr=%g svr=%g svt=%g J=%g E=%g\n",
        index, star_r[index], star[index].vr, star[index].vt, star[index].J, star[index].E);
    orbit_rs.rp = star_r[g_si];
    orbit_rs.ra = star_r[g_si];
    orbit_rs.kmin = g_si;
    orbit_rs.kmax = g_si;
    orbit_rs.dQdrp = 0.0;
    orbit_rs.dQdra = 0.0;
    orbit_rs.circular_flag = 1;
  } else {
    orbit_rs.rp = rmin;
    orbit_rs.ra = rmax;
    orbit_rs.kmin= kmin;
    orbit_rs.kmax= kmax;
    orbit_rs.dQdrp = dQdr_min;
    orbit_rs.dQdra = dQdr_max;
    orbit_rs.circular_flag = 0;	
  };
  return(orbit_rs);
};


/* Do not use for newly created stars! */
/**
* @brief ?
*
* @param index ?
* @param J ?
* @param pos_old ?
* @param orbit_old ?
*
* @return ?
*/
orbit_rs_t calc_orbit_new_J(long index, double J, struct star_coords pos_old, orbit_rs_t orbit_old) {
  orbit_rs_t orbit_rs;
  long ktemp, kmin, kmax;
  double E, dQdrp, dQdra, J_old, rp_old, ra_old, rmin, rmax;
  double a, b, dQdr_min, dQdr_max;
  int circular;

  circular=0;
  E= pos_old.E+ PHI_S(pos_old.r, index);
  J_old= pos_old.J;
  rp_old= orbit_old.rp;
  ra_old= orbit_old.ra;
  kmin= orbit_old.kmin;
  kmax= orbit_old.kmax;

  dQdrp= orbit_old.dQdrp- 2.0*fb_sqr(J_old)/(rp_old*rp_old*rp_old);
  dQdrp+= 2.0*fb_sqr(J)/(rp_old*rp_old*rp_old);
  dQdra= orbit_old.dQdra- 2.0*fb_sqr(J_old)/(ra_old*ra_old*ra_old);
  dQdra+= 2.0*fb_sqr(J)/(ra_old*ra_old*ra_old);

  ktemp= get_positive_Q_index_new_J(index, E, J, orbit_old);
  if (ktemp<=-1) {
    dprintf("Did not find a single positive Q value! index= %li\n", index);
    dprintf("circular orbit found: index=%ld sr=%g svr=%g svt=%g J=%g E=%g\n",
        index, star[index].r, star[index].vr, star[index].vt, star[index].J, star[index].E);
    orbit_rs.rp = pos_old.r;
    orbit_rs.ra = pos_old.r;
    orbit_rs.kmin = index;
    orbit_rs.kmax = index;
    orbit_rs.dQdrp = 0.0;
    orbit_rs.dQdra = 0.0;
    orbit_rs.circular_flag = 1;
    return (orbit_rs);
  }

  /* calculate new kmin */
  if (kmin> clus.N_MAX) kmin= clus.N_MAX;
  if (function_q(index, star[kmin].r, star[kmin].phi, E, J)<0.) {
    if (dQdrp< 0.) {
      if (ktemp<kmin || orbit_old.circular_flag) {
        kmin= find_zero_Q(index, 0, ktemp, E, J);
      } else {
        kmin= find_zero_Q(index, 0, kmin+1, E, J);
      };
    } else {
      kmin= find_zero_Q(index, kmin, ktemp, E, J);
    };
  } else {
    if (ktemp<kmin || orbit_old.circular_flag) {
      kmin= find_zero_Q(index, 0, ktemp, E, J);
    } else {
      kmin= find_zero_Q(index, 0, kmin+1, E, J);
    };
  };

  /* calculate new kmax */
  if (kmax> clus.N_MAX) kmax= clus.N_MAX;
  if (function_q(index, star[kmax].r, star[kmax].phi, E, J)<0.) {
    if (dQdrp< 0. && (!orbit_old.circular_flag)) {
      kmax= find_zero_Q(index, ktemp, kmax+1, E, J);
    } else {
      kmax= find_zero_Q(index, ktemp, clus.N_STAR+ 1, E, J);
    };
  } else {
    if (ktemp>kmax && (!orbit_old.circular_flag)) {
      kmax= find_zero_Q(index, kmax, clus.N_MAX+ 1, E, J);
    } else {
      kmax= find_zero_Q(index, ktemp, clus.N_MAX+ 1, E, J);
    };
  };

/*
 *  ktemp= get_positive_Q_index(index, E, J);
 *  if (ktemp<=-1) {
 *    circular=1;
 *    dprintf("Did not find a single positive Q value! index= %li\n", index);
 *  } else {
 *    if (kmin<=-1) {
 *      dprintf("Use bisection to find kmin\n");
 *      kmin = FindZero_Q(index, 0, ktemp, E, J);
 *      dprintf("kmin= %li\n", kmin);
 *    };
 *    if (kmax<=-1) {
 *      dprintf("Use bisection to find kmax\n");
 *      kmax = FindZero_Q(index, ktemp, clus.N_MAX + 1, E, J);
 *      dprintf("kmax= %li\n", kmax);
 *    };
 *  };
 *
 */
  /* calculate rmin and rmax */
  if (!circular) {
    int rmax_in_interval, rmin_in_interval, vr_rmax_positive, vr_rmin_positive;

    /* First we try Henon's method. If it fails we use bisection. */
    set_a_b(index, kmin, &a, &b);
    rmin = J * J / (-a + sqrt(a * a - 2.0 * J * J * (b - E)));
    dQdr_min = 2.0 * J * J / (rmin * rmin * rmin) + 2.0 * a / (rmin * rmin);

    set_a_b(index, kmax, &a, &b);
    rmax = (-a + sqrt(a * a - 2.0 * J * J * (b - E))) / (2.0 * (b - E));
    dQdr_max = 2.0 * J * J / (rmax * rmax * rmax) + 2.0 * a / (rmax * rmax);
    
    /* Consistency check for rmin and rmax. If it fails, we bisect our way through.*/
    rmin_in_interval= rmin < star[kmin+1].r && rmin > star[kmin].r;
    rmax_in_interval= rmax < star[kmax+1].r && rmax > star[kmax].r;
    vr_rmin_positive= calc_vr_in_interval(rmin, index, kmin, E, J)>= 0.;
    vr_rmax_positive= calc_vr_in_interval(rmax, index, kmax, E, J)>= 0.;

    if (!(rmax_in_interval && vr_rmax_positive)) {
      dprintf("rmax is out of interval (%i) or vr is negative (%i)\n",
          !rmax_in_interval, !vr_rmax_positive);
      rmax= find_root_vr(index, kmax, E, J);
      dprintf("New rmax= %g and residual vr= %g\n", rmax, calc_vr(rmax, index, E, J)); 
      set_a_b(index, kmax, &a, &b);
      dQdr_max = 2.0 * J * J / (rmax * rmax * rmax) + 2.0 * a / (rmax * rmax);
    };

    if (!(rmin_in_interval && vr_rmin_positive)) {
      dprintf("rmin is out of interval (%i) or vr is negative (%i)\n",
          !rmin_in_interval, !vr_rmin_positive);
      rmin= find_root_vr(index, kmin, E, J);
      dprintf("New rmin= %g and residual vr= %g\n", rmin, calc_vr(rmin, index, E, J)); 
      set_a_b(index, kmin, &a, &b);
      dQdr_min = 2.0 * J * J / (rmin * rmin * rmin) + 2.0 * a / (rmin * rmin);
    };
  };
 
  /* another case of a circular orbit */
  if (!circular && (rmin >= rmax)) {
    eprintf("rmin=%g>=rmax=%g: kmin=%ld kmax=%ld index=%ld r=%g vr=%g vt=%g J=%g E=%g Q(kmin)=%g Q(kmax)=%g\n",
        rmin, rmax, kmin, kmax, index, pos_old.r, pos_old.vr, pos_old.vt, pos_old.J, pos_old.E,
        function_Q(index, kmin, pos_old.E, pos_old.J), function_Q(index, kmax, pos_old.E, pos_old.J));
    eprintf("rmin-rmax= %g\n", rmin-rmax);
    circular= 1;
  }

  /* Set the return variables. */
  if (circular) {
    dprintf("circular orbit found: index=%ld sr=%g svr=%g svt=%g J=%g E=%g\n",
        index, star[index].r, star[index].vr, star[index].vt, star[index].J, star[index].E);
    orbit_rs.rp = pos_old.r;
    orbit_rs.ra = pos_old.r;
    orbit_rs.kmin = index;
    orbit_rs.kmax = index;
    orbit_rs.dQdrp = 0.0;
    orbit_rs.dQdra = 0.0;
    orbit_rs.circular_flag = 1;
  } else {
    orbit_rs.rp = rmin;
    orbit_rs.ra = rmax;
    orbit_rs.kmin= kmin;
    orbit_rs.kmax= kmax;
    orbit_rs.dQdrp = dQdr_min;
    orbit_rs.dQdra = dQdr_max;
    orbit_rs.circular_flag = 0;	
  };
  return(orbit_rs);
};

/**
* @brief ?
*
* @param index ?
* @param k ?
* @param a ?
* @param b ?
*/
void set_a_b(long index, long k, double *a, double *b) {
  long i, i1;
  double rk, rk1, Uk, Uk1;

  if (k==0) {
    *a= -cenma.m*madhoc;
    *b= star_phi[1]+ cenma.m*madhoc/star_r[1]+ star_m[index]*madhoc/star_r[index];
  } else {
    i = k;
    i1 = k + 1;
    rk = star_r[i];
    rk1 = star_r[i1];
    Uk = star_phi[i] + MPI_PHI_S(rk, index);
    Uk1 = star_phi[i1]  + MPI_PHI_S(rk1, index);

    *a = (Uk1 - Uk) / (1. / rk1 - 1. / rk);
    *b = (Uk / rk1 - Uk1 / rk) / (1. / rk1 - 1. / rk);
  }
};

/**
* @brief ?
*
* @param index ?
* @param k ?
* @param E ?
* @param J ?
* @param positive ?
*
* @return ?
*/
long find_zero_Q_slope(long index, long k, double E, double J, int positive) {
  double qtemp, pos;
  long ktemp;

  ktemp= k;
  qtemp= function_Q(index, ktemp, E, J);
  if (!positive) {
    pos= -1.;
  } else {
    pos= 1.;
  };
  if (qtemp*pos>= 0.) {
    while (qtemp*pos>=0.) {
      ktemp--;
      qtemp= function_Q(index, ktemp, E, J);
      if (ktemp==0 && qtemp*pos>= 0) {
        ktemp= -1;
        break;
      };
    }
  } else {
    while (qtemp*pos<0.) {
      ktemp++;
      qtemp=function_Q(index, ktemp, E, J);
      if (ktemp> clus.N_MAX) {
        ktemp= -1;
        break;
      };
    }
    ktemp--;
  };

  return(ktemp);
};

