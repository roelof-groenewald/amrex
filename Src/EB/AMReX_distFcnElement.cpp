#include "AMReX_distFcnElement.H"

/* ---------------------------------------------------------------------------*/
/* Implementation for Distance Function 2D Base Class */
/* ---------------------------------------------------------------------------*/

namespace amrex {

int distFcnElement2d::solve_thomas(const std::vector<amrex::Real> &a,
                                   std::vector<amrex::Real> b,
                                   const std::vector<amrex::Real> &c,
                                   std::vector<amrex::Real> d,
                                   std::vector<amrex::Real> &x)
{
  int n = static_cast<int>(d.size());
  x.resize(n);

  amrex::Real m;
  for (int i=1; i < n; ++i) {
    m = a[i-1]/b[i-1];
    b[i] -= m*c[i-1];
    d[i] -= m*d[i-1];
  }

  x[n-1] = d[n-1] / b[n-1];

  for (int i=n-2; i > -1; --i) {
    x[i] = (d[i]-c[i]*x[i+1])/b[i];
  }

  return 0;
}

}


/* ---------------------------------------------------------------------------*/
/* Implementation for Splines */
/* ---------------------------------------------------------------------------*/

namespace amrex {

distFcnElement2d* SplineDistFcnElement2d::newDistFcnElement2d() const {
  auto* newSpline = new SplineDistFcnElement2d();
  newSpline->control_points_x = control_points_x;
  newSpline->control_points_y = control_points_y;
  newSpline->bc_pt_start = bc_pt_start;
  newSpline->bc_pt_end = bc_pt_end;
  newSpline->Dx = Dx;
  newSpline->Dy = Dy;
  return static_cast<distFcnElement2d*>(newSpline);
}

amrex::Real SplineDistFcnElement2d::eval(amrex::Real t,
                                         amrex::Real y0, amrex::Real y1,
                                         amrex::Real D0, amrex::Real D1) {
  amrex::Real c, d;

  c = 3.0_rt*(y1 - y0) - 2.0_rt*D0 - D1;
  d = 2.0_rt*(y0-y1) + D0 + D1;
  return y0 + D0*t + c*t*t + d*t*t*t;
}

amrex::Real SplineDistFcnElement2d::dist(amrex::RealVect pt,
                           amrex::Real x0, amrex::Real x1,
                           amrex::Real Dx0, amrex::Real Dx1,
                           amrex::Real y0, amrex::Real y1,
                           amrex::Real Dy0, amrex::Real Dy1,
                           amrex::Real& t,
                           amrex::RealVect& spt)
{
  amrex::RealVect delta;
  spt[0] = eval(t, x0, x1, Dx0, Dx1);
  spt[1] = eval(t, y0, y1, Dy0, Dy1);

  delta = spt - pt;

  amrex::Real dist;
  dist = std::sqrt(delta[0]*delta[0] + delta[1]*delta[1]);

  return dist;
}


amrex::Real SplineDistFcnElement2d::cpdist(amrex::RealVect pt,
                                           amrex::RealVect & cpmin) const {
  auto dmin = Real(1.0e29);
  amrex::Real t;
  amrex::RealVect cp;
  amrex::Real dist;
  int nsplines = static_cast<int>(Dx.size() - 1);
  for (int i=0; i<nsplines; ++i) {
    single_spline_cpdist(pt, control_points_x[i], control_points_x[i+1],
                         Dx[i], Dx[i+1],
                         control_points_y[i], control_points_y[i+1],
                         Dy[i], Dy[i+1],
                         t, cp, dist);
    if (dist < dmin) {
      dmin = dist;
      cpmin = cp;
    }
  }
  return dmin;
}


amrex::Real SplineDistFcnElement2d::cpside(amrex::RealVect pt,
                                           amrex::RealVect & cpmin)
                                           const {
  auto dmin = Real(1.0e29);
  amrex::Real t;
  amrex::RealVect cp;
  amrex::Real dist;
  amrex::Real x0=0., x1=0., y0=0., y1=0., Dx0=0., Dx1=0., Dy0=0., Dy1=0., tmin=0.;
  int nsplines = static_cast<int>(Dx.size() - 1);
  for (int i=0; i<nsplines; ++i) {
    single_spline_cpdist(pt, control_points_x[i], control_points_x[i+1],
                         Dx[i], Dx[i+1],
                         control_points_y[i], control_points_y[i+1],
                         Dy[i], Dy[i+1],
                         t, cp, dist);
    if (dist < dmin) {
      dmin = dist;
      cpmin = cp;
      tmin = t;
      x0 = control_points_x[i];
      x1 = control_points_x[i+1];
      y0 = control_points_y[i];
      y1 = control_points_y[i+1];
      Dx0 = Dx[i];
      Dx1 = Dx[i+1];
      Dy0 = Dy[i];
      Dy1 = Dy[i+1];
    }
  }

  // Now that we've found the closest spline, find which side
  // it is on
  amrex::RealVect A = pt - cpmin;
  amrex::RealVect B;
  amrex::Real dx, dx2, dy, dy2;

  auto tangentDist = Real(0.001);
  if (dmin < tangentDist) {
    // by the cross product
    // with tangent and vector to point
    dxbydt(tmin, x0, x1, Dx0, Dx1, dx, dx2);
    dxbydt(tmin, y0, y1, Dy0, Dy1, dy, dy2);
    B[0] = dx;
    B[1] = dy;
  } else {
    // If 'far', by cross product with line between control points
    B[0] = x1-x0;
    B[1] = y1-y0;
  }

  amrex::Real AcrossB = A[0]*B[1] - A[1]*B[0];

  amrex::Real side;
  if (AcrossB < 0) {
    side = Real(1.0);
  } else if (AcrossB > 0) {
    side = Real(-1.0);
  } else {
   side = Real(0.0);
  }
  return side;
}


void SplineDistFcnElement2d::single_spline_cpdist(amrex::RealVect pt,
                                    amrex::Real x0, amrex::Real x1,
                                    amrex::Real Dx0, amrex::Real Dx1,
                                    amrex::Real y0, amrex::Real y1,
                                    amrex::Real Dy0, amrex::Real Dy1,
                                    amrex::Real& t, amrex::RealVect& mincp,
                                    amrex::Real& mindist)
{
  t = Real(0.5);

  amrex::RealVect spt, deltapt;
  const int maxIters = 1;

  amrex::Real dxf, d2xf, dyf, d2yf, tnew;

  for (int i=0; i<maxIters; ++i) {
    mindist = dist(pt, x0, x1, Dx0, Dx1, y0, y1, Dy0, Dy1, t, spt);
    dxbydt(t, y0, y1, Dy0, Dy1, dyf, d2yf);
    dxbydt(t, x0, x1, Dx0, Dx1, dxf, d2xf);

    deltapt =  spt - pt;
    tnew = t - (deltapt[0]*dxf + deltapt[1]*dyf) / (
      dxf*dxf + dyf*dyf + deltapt[0]*d2xf + deltapt[1]*d2yf);

    if (tnew < Real(0.0)) {
      tnew = Real(0.0);
    } else if (tnew > Real(1.0)) {
      tnew = Real(1.0);
    }
    t = tnew;
  }
  mindist = dist(pt, x0, x1, Dx0, Dx1, y0, y1, Dy0, Dy1, tnew, spt);
  mincp = spt;

  if (mindist == Real(0.0)) {
    std::cout << "identified minimum distance of 0.0 at t = " << t
              << "; cp = " << mincp << " for p = " << pt << '\n';
  }
}


void SplineDistFcnElement2d::dxbydt(amrex::Real t,
                                    amrex::Real y0, amrex::Real y1,
                                    amrex::Real D0, amrex::Real D1,
                                    amrex::Real& dyf, amrex::Real& d2yf)
{
  amrex::Real c, d;
  c = Real(3.0)*(y1 - y0) - Real(2.0)*D0 - D1;
  d = Real(2.0)*(y0-y1) + D0 + D1;

  dyf = D0 + Real(2.0)*c*t + Real(3.0)*d*t*t;
  d2yf = Real(2.0)*c + Real(6.0)*d*t;
}


void SplineDistFcnElement2d::calc_D(bool clamped_bc)
{
  int nsplines = static_cast<int>(control_points_x.size() - 1);

  std::vector<amrex::Real> rhsx, rhsy, diag, diagminus, diagplus;
  rhsx.resize(nsplines+1);
  rhsy.resize(nsplines+1);
  diag.resize(nsplines+1);
  diagminus.resize(nsplines);
  diagplus.resize(nsplines);

  Dx.resize(nsplines+1);
  Dy.resize(nsplines+1);

  for (int i=0; i<nsplines; ++i) {
    diag[i] = Real(4.0);
    diagminus[i] = Real(1.0);
    diagplus[i] = Real(1.0);
  }

  for (int i=1; i<nsplines; ++i) {
    rhsx[i] = Real(3.0)*(control_points_x[i+1] - control_points_x[i-1]);
    rhsy[i] = Real(3.0)*(control_points_y[i+1] - control_points_y[i-1]);
  }

  if (clamped_bc) {
    diag[0] = Real(1.0);
    diagminus[0] = Real(0.0);

    diag[nsplines] = Real(1.0);
    diagplus[nsplines] = Real(0.0);

    rhsx[0] = control_points_x[0] - bc_pt_start[0];
    rhsx[nsplines] = -control_points_x[nsplines-1] + bc_pt_end[0];

    rhsy[0] = control_points_y[0] - bc_pt_start[1];
    rhsy[nsplines] = -control_points_y[nsplines-1] + bc_pt_end[1];

  } else {
    // Natural boundary conditions
    diag[0] = Real(2.0);
    diag[nsplines] = Real(2.0);

    rhsx[0] = Real(3.0)*(control_points_x[1]-control_points_x[0]);
    rhsx[nsplines] = Real(3.0) * (control_points_x[nsplines] -
                            control_points_x[nsplines-1]);

    rhsy[0] = Real(3.0)*(control_points_y[1]-control_points_y[0]);
    rhsy[nsplines] = Real(3.0) * (control_points_y[nsplines] -
                            control_points_y[nsplines-1]);
  }

  solve_thomas(diagminus, diag, diagplus, rhsx, Dx);
  solve_thomas(diagminus, diag, diagplus, rhsy, Dy);
}



void SplineDistFcnElement2d::set_control_points
(std::vector<amrex::RealVect> pts) {
  for (auto & pt : pts) {
    control_points_x.push_back(pt[0]);
    control_points_y.push_back(pt[1]);
    //  std::cout << "Added point (" << pt[0] << "," << pt[1] << ")" << std::endl;
  }
}


void SplineDistFcnElement2d::set_bc_points(amrex::RealVect start,
                                           amrex::RealVect end) {
  bc_pt_start = start;
  bc_pt_end = end;
}


void SplineDistFcnElement2d::print_control_points() const {
  for (unsigned i=0; i<control_points_x.size(); ++i) {
    std::cout << "(" << control_points_x[i] << ","
              << control_points_y[i] << ")" << '\n';
  }

  std::cout << "(" << bc_pt_start[0] << ","
            << bc_pt_start[1] << ")" << '\n';
  std::cout << "(" << bc_pt_end[0] << "," << bc_pt_end[1] << ")" << '\n';
}


void SplineDistFcnElement2d::print_spline() const {
#if 0
  int nsplines = Dx.size();

  amrex::Real dt = Real(0.01);
  int nt = static_cast<int>(Real(1.0)/dt);
  for (int i=0; i<nsplines-1; i++) {
    for (int j=0; j<nt; j++) {
      amrex::Real x = eval(j*dt, control_points_x[i],
                           control_points_x[i+1], Dx[i], Dx[i+1]);
      amrex::Real y = eval(j*dt, control_points_y[i],
                           control_points_y[i+1], Dy[i], Dy[i+1]);

    }
  }
#endif
}


/* ---------------------------------------------------------------------------*/
/* Implementation for Lines */
/* ---------------------------------------------------------------------------*/
amrex::Real LineDistFcnElement2d::cpdist(amrex::RealVect pt,
                                         amrex::RealVect & cpmin) const {

  amrex::Real mindist, dist;
  mindist = Real(1.0e29);
  amrex::RealVect cp;

  for (int i=1, N=static_cast<int>(control_points_x.size()); i<N; ++i) {
    single_seg_cpdist(pt,
                      control_points_x[i-1], control_points_x[i],
                      control_points_y[i-1], control_points_y[i],
                      cp, dist);
    if (dist < mindist) {
      mindist = dist;
      cpmin = cp;
    }
  }
  return mindist;

}
amrex::Real LineDistFcnElement2d::cpside(amrex::RealVect pt,
                                         amrex::RealVect & cpmin) const {

  amrex::Real mindist, dist;
  mindist = Real(1.0e29);
  amrex::RealVect cp;
  amrex::RealVect l0, l1;

  for (int i=1, N=static_cast<int>(control_points_x.size()); i<N; ++i) {
    single_seg_cpdist(pt,
                      control_points_x[i-1], control_points_x[i],
                      control_points_y[i-1], control_points_y[i],
                      cp, dist);
    if (dist < mindist) {
      mindist = dist;
      cpmin = cp;
      l0 = amrex::RealVect(AMREX_D_DECL(control_points_x[i-1], control_points_y[i-1],Real(0.0)));
      l1 = amrex::RealVect(AMREX_D_DECL(control_points_x[i], control_points_y[i],Real(0.0)));
    }
  }

  //amrex::RealVect B = l1 - cpmin;
  amrex::RealVect B = l1 - l0;
  amrex::RealVect A = pt - cpmin;
  amrex::Real AcrossB = A[0]*B[1] - A[1]*B[0];

  if (AcrossB < Real(0.0)) {
    return Real(1.0);
  } else if (AcrossB > Real(0.0)) {
    return Real(-1.0);
  } else {
    return Real(0.0);
    /*
    // AcrossB is zero, try extending line segment
    if (B[0]*B[0] + B[1]*B[1] != 0.0) {
      amrex::RealVect cext = amrex::RealVect(l1 - l0)*1.05 + l0;
      B = cext - cpmin;
      AcrossB = A[0]*B[1] - A[1]*B[0];
      if (AcrossB < 0.0) {
        return 1.0;
      } else if (AcrossB > 0.0) {
        return -1.0;
      }
    } else {
        // Point must be on the line, put it inside
        return 0.0;
    }
    */
  }
}

void LineDistFcnElement2d::set_control_points
(std::vector<amrex::RealVect> pts) {
  for (auto & pt : pts) {
    control_points_x.push_back(pt[0]);
    control_points_y.push_back(pt[1]);
    //  std::cout << "Added point (" << pt[0] << "," << pt[1] << ")" << std::endl;
  }
}

void LineDistFcnElement2d::single_seg_cpdist(amrex::RealVect pt,
                                             amrex::Real x0, amrex::Real x1,
                                             amrex::Real y0, amrex::Real y1,
                                             amrex::RealVect& cp,
                                             amrex::Real& dist) {
  amrex::RealVect A(AMREX_D_DECL(pt[0]-x0, pt[1]-y0,Real(0.0)));
  amrex::RealVect B(AMREX_D_DECL(x1-x0, y1-y0,Real(0.0)));

  amrex::Real magBsq = B[0]*B[0] + B[1]*B[1];
  amrex::Real t =  (A[0]*B[0] + A[1]*B[1])/magBsq;

  if (t < 0) {
    cp = amrex::RealVect(AMREX_D_DECL(x0,y0,Real(0.0)));
  } else if (t > Real(1.0)) {
    cp = amrex::RealVect(AMREX_D_DECL(x1,y1,Real(0.0)));
  } else {
    cp  = amrex::RealVect(AMREX_D_DECL(x0,y0,Real(0.0))) + t*B;
  }

  amrex::RealVect delta = pt - cp;
  dist = std::sqrt(delta[0]*delta[0] + delta[1]*delta[1] );
}

void LineDistFcnElement2d::print_control_points()
{
  for (int i=1, N=static_cast<int>(control_points_x.size()); i<N; ++i) {
    std::cout << "(" << control_points_x[i-1] << ", "<< control_points_y[i-1] << ")" << "---"
              << "(" << control_points_x[i] << ", " << control_points_y[i] << ")" << '\n';
  }
}

distFcnElement2d* LineDistFcnElement2d::newDistFcnElement2d() const {
  auto* newLine = new LineDistFcnElement2d();
  newLine->control_points_x = control_points_x;
  newLine->control_points_y = control_points_y;
  return static_cast<distFcnElement2d*>(newLine);
}

}
