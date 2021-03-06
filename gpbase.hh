#ifndef GPBASE_HH
#define GPBASE_HH

// JCC: The file did not have the #include lines
//#include "gpbase.hh"
//#include <gsl/gsl_rng.h>
#include <string>
#include <iostream>
#include <assert.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_sf_psi.h>
#include <gsl/gsl_sf_gamma.h>
#include "env.hh"
using namespace std;

template <class T>
class GPBase {
public:
  GPBase(string name = ""): _name(name) { }
  virtual ~GPBase() { }
  virtual const T &expected_v() const = 0;
  virtual const T &expected_logv() const = 0;
  virtual uint32_t n() const = 0;
  virtual uint32_t k() const = 0;
  virtual double compute_elbo_term_helper() const = 0;
  virtual void save_state(const IDMap &m, string filename) const = 0;
  virtual void load()  = 0;
  virtual void load_from_lda(string dir, double alpha, uint32_t K) 
  { lerr("load_from_lda() unimplemented!\n"); }
  void  make_nonzero(double av, double bv,
		     double &a, double &b) const;
  string name() const { return _name; }
  double compute_elbo_term() const;
private:
  string _name;
};
typedef GPBase<Matrix> GPM;

template<class T> inline  void
GPBase<T>::make_nonzero(double av, double bv,
			double &a, double &b) const
{
//  cout << av << " " << bv << endl;
  if (!(av >= 0 && bv >= 0)) {
    lerr("av = %f, bv = %f", av, bv);
    assert(0);
  }
  if (!(bv > .0)) 
    b = 1e-30;
  else
    b = bv;

  if (!(av > .0)) 
    a = 1e-30;
  else
    a = av;
}

template<class T> inline  double
GPBase<T>::compute_elbo_term() const
{
  double s = compute_elbo_term_helper();
  debug("sum of %s elbo terms = %f\n", name().c_str(), s);
  return s;
}

// Matrix of gamma random variables
// prior refers to the hyperparameters
// curr is the current value, next the next value
class GPMatrix : public GPBase<Matrix> {
public:
  // Constructor with constant parameters
  GPMatrix(string name, double a, double b,
	   uint32_t n, uint32_t k,
	   gsl_rng **r): 
    GPBase<Matrix>(name),
    _n(n), _k(k),
    _sprior(a), // shape 
    _rprior(k,b), // rate
    _hier(false),
    _hier_rprior(n),
    _hier_log_rprior(n),
    _scurr(n,k),
    _snext(n,k),
    _rnext(n,k),
    _rcurr(n,k),
    _Ev(n,k),
    _Elogv(n,k),
    _r(r) {
//      cout << "here" << endl;
//      double** mat = _scurr.data();
//      cout << k << " " << n << endl;
//      _rprior.print();
//      cout << mat[k][n] << endl;
    }
  
  //Constructor with an array of rate parameters
  GPMatrix(string name, double a, double b, Array & rateScale,
           uint32_t n, uint32_t k,
           gsl_rng **r):
  GPBase<Matrix>(name),
  _n(n), _k(k),
  _sprior(a), // shape
  _rprior(rateScale.size()), // rate
  _hier(false),
  _hier_rprior(n),
  _hier_log_rprior(n),
  _scurr(n,k),
  _snext(n,k),
  _rnext(n,k),
  _rcurr(n,k),
  _Ev(n,k),
  _Elogv(n,k),
  _r(r) {
    _rprior.copy_from(rateScale);
    _rprior.scale(b);
    
//     cout << "here" << endl;
//     double** mat = _scurr.data();
//     cout << mat[k][n] << endl;
    
  }
  
//  //Constructor with an array of rate parameters and a variable for popularity/activity
//  GPMatrix(string name, double a, double b, Array & rateScale, double popAct,
//           uint32_t n, uint32_t k,
//           gsl_rng **r):
//  GPBase<Matrix>(name),
//  _n(n), _k(k),
//  _sprior(a), // shape
//  _rprior(rateScale.size()), // rate
//  _hier(false),
//  _hier_rprior(n),
//  _hier_log_rprior(n),
//  _scurr(n,k),
//  _snext(n,k),
//  _rnext(n,k),
//  _rcurr(n,k),
//  _Ev(n,k),
//  _Elogv(n,k),
//  _r(r) {
//    _rprior.copy_from(rateScale);
//    _rprior.scale(b);
//    _rprior.scale(1/popAct);
//    
//    //     cout << "here" << endl;
//    //     double** mat = _scurr.data();
//    //     cout << mat[k][n] << endl;
//    
//  }
  
  virtual ~GPMatrix() { }

  uint32_t n() const { return _n;}
  uint32_t k() const { return _k;}

  void save() const;
  void load();

  const Matrix &shape_curr() const         { return _scurr; }
  const Matrix &rate_curr() const          { return _rcurr; }
  const Matrix &shape_next() const         { return _snext; }
  const Matrix &rate_next() const          { return _rnext; }
  const Matrix &expected_v() const         { return _Ev;    }
  const Matrix &expected_logv() const      { return _Elogv; }
  
  void expected_means(Array & means) const;
  
  Matrix &shape_curr()       { return _scurr; }
  Matrix &rate_curr()        { return _rcurr; }
  Matrix &shape_next()       { return _snext; }
  Matrix &rate_next()        { return _rnext; }
  Matrix &expected_v()       { return _Ev;    }
  Matrix &expected_logv()    { return _Elogv; }

  const double sprior() const { return _sprior; }
  const Array rprior() const { return _rprior; }

  void set_to_prior();
  void set_to_prior_curr();

  void update_shape_next1(uint32_t n, const Array &sphi);
  void update_shape_next2(uint32_t n, const uArray &sphi);
  void update_shape_curr(uint32_t n, const uArray &sphi);
  void update_shape_next3(uint32_t n, uint32_t k, double v);

  void update_rate_next(const Array &u, const Array &scale);
  void update_rate_next(const Array &u);
  void update_rate_next(const Array &u, double scale);
  void update_rate_curr(const Array &u);
  void update_rate_next_all(uint32_t k, double v);
  void update_rate_next(uint32_t n, const Array &u);

  void swap();
  void compute_expectations();
  void sum_rows(Array &v); 
  void sum_available_rows(Array &avbl,Array &v);
  void scaled_sum_rows(Array &v, const Array &scale);
  void sum_cols(Array &v);
  void sum_cols_weight(const Array & weights,Array &v);
  void initialize(double offset);

  void initialize2(double v, double offset);
  void initialize_exp(double offset);
  void initialize_exp(double v, double offset);
  void save_state(const IDMap &m, string filename) const;
  void load_from_lda(string dir, double alpha, uint32_t K);
  void set_prior_rate(const Array &ev, const Array &elogv);
  void set_prior_rate_scaled(const Array &ev, const Array &elogv, Array &scale);
  void set_prior_rate_scaled(const Array &ev, double factor, Array &scale);


  double compute_elbo_term_helper() const;

private:
  uint32_t _n;
  uint32_t _k;	
  gsl_rng **_r;
  double _sprior;
  Array _rprior;

  bool _hier;
  Array _hier_rprior;
  Array _hier_log_rprior;

  Matrix _scurr;      // current variational shape posterior 
  Matrix _snext;      // to help compute gradient update
  Matrix _rcurr;      // current variational rate posterior (global)
  Matrix _rnext;      // help compute gradient update
  Matrix _Ev;         // expected weights under variational
		      // distribution
  Matrix _Elogv;      // expected log weights 
};

// Sets parameters of next iteration to prior values
inline void
GPMatrix::set_to_prior()
{
  _snext.set_elements(_sprior);
  _rnext.set_rows(_rprior);
}

inline void
GPMatrix::set_to_prior_curr()
{
  _scurr.set_elements(_sprior);
  _rcurr.set_rows(_rprior);
}

// Sets in the prior rate
inline void
GPMatrix::set_prior_rate(const Array &ev, const Array &elogv)
{
  assert (ev.size() == _n && elogv.size() == _n);
  for (uint32_t n = 0; n < _n; ++n) {
    _rnext.set_row(n, ev[n]);
    _hier_rprior[n] = ev[n];
    _hier_log_rprior[n] = elogv[n];
  }
  _hier = true;
}

// Saves
inline void
GPMatrix::set_prior_rate_scaled(const Array &ev, const Array &elogv, Array &scale)
{
  assert (ev.size() == _n && elogv.size() == _n);
  assert(scale.size() == _k);
  for (uint32_t n = 0; n < _n; ++n) {
    for ( uint32_t k = 0; k < _k; ++k) {
      _rnext.set(n,k, ev[n]*scale[k]);
      //Not sure what the next two lines do
//    _hier_rprior[n] = ev[n];
//    _hier_log_rprior[n] = elogv[n];
    }
  }
  _hier = true;
}

// Saves
inline void
GPMatrix::set_prior_rate_scaled(const Array &ev, double factor, Array &scale)
{
  assert (ev.size() == _n);
  assert(scale.size() == _k);
  for (uint32_t n = 0; n < _n; ++n) {
    for ( uint32_t k = 0; k < _k; ++k) {
      _rnext.set(n,k, factor * ev[n] * scale[k]);
      //Not sure what the next two lines do
      //    _hier_rprior[n] = ev[n];
      //    _hier_log_rprior[n] = elogv[n];
    }
  }
  _hier = true;
}

// Adds sphi to the nth row of this
inline void
GPMatrix::update_shape_next1(uint32_t n, const Array &sphi)
{
  _snext.add_slice(n, sphi);
  //printf("snext = %s\n", _snext.s().c_str());
}

inline void
GPMatrix::update_shape_next2(uint32_t n, const uArray &sphi)
{
  _snext.add_slice(n, sphi);
}

inline void
GPMatrix::update_shape_next3(uint32_t n, uint32_t k, double v)
{
  double **snextd = _snext.data();
  snextd[n][k] += v;
}

inline void
GPMatrix::update_shape_curr(uint32_t n, const uArray &sphi)
{
  _scurr.add_slice(n, sphi);
}

//inline void
//GPMatrix::update_rate_next(const Array &u, const Array &scale)
//{
//  Array t(_k);
//  for (uint32_t i = 0; i < _n; ++i) {
//    for (uint32_t k = 0; k < _k; ++k)
//      t[k] = u[k] * scale[i];
//    _rnext.add_slice(i, t);
//  }
//}

// Updates the next rate with a scaled array,
inline void
GPMatrix::update_rate_next(const Array &u, const Array &scale)
{
  Array t(_k);
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k)
      t[k] = u[k] * scale[i];
    _rnext.add_slice(i, t);
  }
}

// Adds u to the nth row of this
inline void
GPMatrix::update_rate_next(uint32_t n, const Array &u)
{
  _rnext.add_slice(n, u);
}

// Adds u to every row of this
inline void
GPMatrix::update_rate_next(const Array &u)
{
  for (uint32_t i = 0; i < _n; ++i)
    _rnext.add_slice(i, u);
}

inline void
GPMatrix::update_rate_next(const Array &u, double scale)
{
  for (uint32_t i = 0; i < _n; ++i)
    _rnext.add_slice(i, u, scale);
}

inline void
GPMatrix::update_rate_next_all(uint32_t k, double v)
{
  double **rd = _rnext.data();
  for (uint32_t i = 0; i < _n; ++i)
    rd[i][k] += v;
}

inline void
GPMatrix::update_rate_curr(const Array &u)
{
  for (uint32_t i = 0; i < _n; ++i)
    _rcurr.add_slice(i, u);
}

// Swaps the current and the next values for the parameters
inline void
GPMatrix::swap()
{
  _scurr.swap(_snext);
  _rcurr.swap(_rnext);
  
  // Sets the next values at the prior, so that the next step would be to add values to it in the next iteration
  set_to_prior();
}

inline void
GPMatrix::compute_expectations()
{
  const double ** const ad = _scurr.const_data();
  const double ** const bd = _rcurr.const_data();
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();
  double a = .0, b = .0;
  for (uint32_t i = 0; i < _scurr.m(); ++i)
    for (uint32_t j = 0; j < _rcurr.n(); ++j) {
//      cout << ad[i][j] << " " << bd[i][j] << endl;
      make_nonzero(ad[i][j], bd[i][j], a, b);
      vd1[i][j] = a / b;
      vd2[i][j] = gsl_sf_psi(a) - log(b);
    }
}

inline void
GPMatrix::sum_rows(Array &v)
{
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[k] += ev[i][k];
}

//New function: sum_available_rows
inline void
GPMatrix::sum_available_rows(Array &avbl, Array &v)
{
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i){
    for (uint32_t k = 0; k < _k; ++k){
      v[k] += avbl[i]*ev[i][k];
    }
  }
}

inline void
GPMatrix::sum_cols(Array &v)
{
  assert(v.size()==_n);
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[i] += ev[i][k];
}

// Sums columns and saves them as an arry
inline void
GPMatrix::sum_cols_weight(const Array &weights,Array &v) {
  assert(v.size()==_n);
  assert(weights.size()==_k);
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k) {
      v[i] += weights.get(k)*ev[i][k];
    }
  }
}

inline void
GPMatrix::scaled_sum_rows(Array &v, const Array &scale)
{
  assert(scale.size() == n() && v.size() == k());
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[k] += ev[i][k] * scale[i];
}

// Sets the current parameters for the first iteration, at the hyperparameters plus a random shock
inline void
GPMatrix::initialize(double offset)
{
  // Gets the matrices of current parameters to modify them
  double **ad = _scurr.data();
  double **bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      // Initial shape values: hyperparameter plus a small random shock
       ad[i][k] = _sprior * (1 + offset * 0.01 * gsl_ran_ugaussian(*_r));

  for (uint32_t k = 0; k < _k; ++k) {
    // Initial rate values are also hyperparameters plus a small shock
    bd[0][k] = _rprior[k]*(1+offset* 0.01 * gsl_ran_ugaussian(*_r));
  }
  
  // Copy the values along user/item
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k) {
      bd[i][k] = bd[0][k];
    }
  }
  set_to_prior();
}

// Sets the current rate parameters for the first iteration, at the hyperparameters plus a random shock. The shape parameters are set at the prior plus argument v
inline void
GPMatrix::initialize2(double v, double offset)
{
  // Gets the matrices of current parameters to modify them
  double **ad = _scurr.data();
  double **bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k) {
      // Initial values: hyperparameter plus a small random shock
      ad[i][k] = _sprior + offset*0.01 * gsl_ran_ugaussian(*_r);
      // Prior plus argument v, which in the paper is Ka or Kc
      bd[i][k] = _rprior[k] + v;
    }
  }
  set_to_prior();
}

// Sets the means of user and item attributes and the log means ( used for the vector of parameters for the multinomial distribution, phi in the paper) for the first iteration, at the computed values from previous parameters plus a small shock
inline void
GPMatrix::initialize_exp(double offset)
{
  // Gets the matrix of current parameters which should have been initialized already
  double **ad = _scurr.data();
  
  // Gets the matrices of means and log means to modify them
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  // This array will hold the initial values of the rate parameters
  Array b(_k);  
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k) {
      // Initial value: prior plus random shock (why do it again?)
      b[k] = _rprior[k]*(1+offset * 0.01 * gsl_ran_ugaussian(*_r));
      assert(b[k]);
      
      // Means: shape/rate parameter
      vd1[i][k] = ad[i][k] / b[k];
      // Log mean: From expectation of a Gamma r.v.
      vd2[i][k] = gsl_sf_psi(ad[i][k]) - log(b[k]);
    }
  set_to_prior();
} 

inline void
GPMatrix::initialize_exp(double v, double offset)
{
  double **ad = _scurr.data();
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  Array b(_k);  
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k) {
      b[k] = v + offset * 0.01 * gsl_ran_ugaussian(*_r);
      assert(b[k]);
      vd1[i][k] = ad[i][k] / b[k];
      vd2[i][k] = gsl_sf_psi(ad[i][k]) - log(b[k]);
    }
  set_to_prior();
} 

inline double
GPMatrix::compute_elbo_term_helper() const
{
  const double **etheta = _Ev.data();
  const double **elogtheta = _Elogv.data();
  const double ** const ad = shape_curr().const_data();
  const double ** const bd = rate_curr().const_data();

  double s = .0;
  for (uint32_t n = 0; n < _n; ++n)  {
    for (uint32_t k = 0; k < _k; ++k) {
      if (_hier) {
	s += _sprior * _hier_log_rprior[n] + (_sprior - 1) * elogtheta[n][k];
	s -= _hier_rprior[n] * etheta[n][k] + gsl_sf_lngamma(_sprior);
      } else {
//	s += _sprior * log(_rprior) + (_sprior - 1) * elogtheta[n][k];
//	s -= _rprior * etheta[n][k] + gsl_sf_lngamma(_sprior);
      }
    }
    double a = .0, b = .0;
    for (uint32_t k = 0; k < _k; ++k) {
      make_nonzero(ad[n][k], bd[n][k], a, b);
      s -= a * log(b) + (a - 1) * elogtheta[n][k];
      s += b * etheta[n][k] + gsl_sf_lngamma(a);
    }
  }
  return s;
}

inline void
GPMatrix::save_state(const IDMap &m, string filename) const
{
  string expv_fname = string("/") + name() + ".tsv";
  string shape_fname = string("/") + name() + "_shape.tsv";
  string rate_fname = string("/") + name() + "_rate.tsv";
  _scurr.save(filename+"/"+Env::outfile_str(shape_fname), m);
  _rcurr.save(filename+"/"+Env::outfile_str(rate_fname), m);
  _Ev.save(filename+"/"+Env::outfile_str(expv_fname), m);
}

inline void
GPMatrix::load()
{
  string fname = name() + ".tsv";
  _Ev.load(fname);
}

inline void
GPMatrix::load_from_lda(string dir, double alpha, uint32_t K)
{
  char buf[1024];
  sprintf(buf, "%s/lda-fits/%s-lda-k%d.tsv", dir.c_str(), name().c_str(), K);
  lerr("loading from %s", buf);
  _Ev.load(buf, 0);
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  Array b(_k);  
  for (uint32_t i = 0; i < _n; ++i) {
    double s = _Ev.sum(i) + alpha * _k;
    for (uint32_t k = 0; k < _k; ++k) {
      double counts = vd1[i][k];
      vd1[i][k] = (alpha + counts) / s;
      vd2[i][k] = gsl_sf_psi(alpha + counts) - gsl_sf_psi(s);
    }
  }
  IDMap m;
  string expv_fname = string("/") + name() + ".tsv";
  _Ev.save(Env::file_str(expv_fname), m);
}

// Saves the means of the expected values over users/items
inline void
GPMatrix::expected_means(Array & means) const {
//  cout << means.size() << endl;
//  cout << _k << endl;
  assert(means.size()==_k);
  _Ev.colmeans(means);
}

class GPMatrixGR : public GPBase<Matrix> { // global rates
public:
  GPMatrixGR(string name, 
	     double a, double b,
	     uint32_t n, uint32_t k,
	     gsl_rng **r):
    GPBase<Matrix>(name),
    _n(n), _k(k),
    _sprior(a), // shape 
    _rprior(b), // rate
    _scurr(n,k),
    _snext(n,k),
    _rnext(k),
    _rcurr(k),
    _Ev(n,k),
    _Elogv(n,k),
    _r(r) { } 
  virtual ~GPMatrixGR() {} 

  uint32_t n() const { return _n;}
  uint32_t k() const { return _k;}

  const Matrix &shape_curr() const         { return _scurr; }
  const Array  &rate_curr() const          { return _rcurr; }
  const Matrix &shape_next() const         { return _snext; }
  const Array  &rate_next() const          { return _rnext; }
  const Matrix &expected_v() const         { return _Ev;    }
  const Matrix &expected_logv() const      { return _Elogv; }
  
  Matrix &shape_curr()       { return _scurr; }
  Array  &rate_curr()        { return _rcurr; }
  Matrix &shape_next()       { return _snext; }
  Array  &rate_next()        { return _rnext; }
  Matrix &expected_v()       { return _Ev;    }
  Matrix &expected_logv()    { return _Elogv; }

  const double sprior() const { return _sprior; }
  const double rprior() const { return _rprior; }

  void set_to_prior();
  void set_to_prior_curr();
  void update_shape_next(const Array &phi);
  void update_shape_next(uint32_t n, const Array &sphi);
  void update_shape_next(uint32_t n, const uArray &sphi);
  void update_shape_curr(uint32_t n, const uArray &sphi);

  void update_shape_next(uint32_t n, uint32_t k, double v);

  void update_rate_next(const Array &u);
  void update_rate_curr(const Array &u);
  void swap();
  void compute_expectations();
  void sum_rows(Array &v);
  void scaled_sum_rows(Array &v, const Array &scale);
  void initialize(double offset);
  void initialize2(double v, double offset);
  void initialize_exp(double offset);
  void initialize_exp(double v, double offset);
  double compute_elbo_term_helper() const;
  void sum_cols(Array &v);

  void save_state(const IDMap &m, string filename) const;
  void load();
  void load_from_lda(string dir, double alpha, uint32_t K);

private:
  uint32_t _n;
  uint32_t _k;	
  gsl_rng **_r;

  double _sprior;
  double _rprior;
  Matrix _scurr;      
  Matrix _snext;      
  Array _rcurr;       
  Array _rnext;       
  Matrix _Ev;         
  Matrix _Elogv;      
};

inline void
GPMatrixGR::set_to_prior()
{
  _snext.set_elements(_sprior);
  _rnext.set_elements(_rprior);
}

inline void
GPMatrixGR::set_to_prior_curr()
{
  _scurr.set_elements(_sprior);
  _rcurr.set_elements(_rprior);
}

inline void
GPMatrixGR::update_shape_next(uint32_t n, const Array &sphi)
{
  _snext.add_slice(n, sphi);
}

inline void
GPMatrixGR::update_shape_next(uint32_t n, const uArray &sphi)
{
  _snext.add_slice(n, sphi);
}

inline void
GPMatrixGR::update_shape_next(uint32_t n, uint32_t k, double v)
{
  double **snextd = _snext.data();
  snextd[n][k] += v;
}

inline void
GPMatrixGR::update_shape_curr(uint32_t n, const uArray &sphi)
{
  _scurr.add_slice(n, sphi);
}

inline void
GPMatrixGR::update_rate_next(const Array &u)
{
  _rnext += u;
}


inline void
GPMatrixGR::update_rate_curr(const Array &u)
{
  _rcurr += u;
}

inline void
GPMatrixGR::swap()
{
  _scurr.swap(_snext);
  _rcurr.swap(_rnext);
  set_to_prior();
}

inline void
GPMatrixGR::compute_expectations()
{
  const double ** const ad = _scurr.const_data();
  const double * const bd = _rcurr.const_data();
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();
  double a = .0, b = .0;
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t j = 0; j < _k; ++j) {
      make_nonzero(ad[i][j], bd[j], a, b);
      vd1[i][j] = a / b;
      vd2[i][j] = gsl_sf_psi(a) - log(b);
    }
  debug("name = %s, scurr = %s, rcurr = %s, Ev = %s\n",
	name().c_str(),
	_scurr.s().c_str(),
	_rcurr.s().c_str(),
	_Ev.s().c_str());
}

inline void
GPMatrixGR::sum_rows(Array &v)
{
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[k] += ev[i][k];
}

inline void
GPMatrixGR::sum_cols(Array &v)
{
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[i] += ev[i][k];
}

inline void
GPMatrixGR::scaled_sum_rows(Array &v, const Array &scale)
{
  assert(scale.size() == n() && v.size() == k());
  const double **ev = _Ev.const_data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k)
      v[k] += ev[i][k] * scale[i];
}

inline void
GPMatrixGR::initialize(double offset)
{
  /* 
  double **ad = _scurr.data();
  double *bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k) 
      ad[i][k] = _sprior + 0.01 * gsl_ran_ugaussian(*_r);

  for (uint32_t k = 0; k < _k; ++k)   
    bd[k] = _rprior + 0.01 * gsl_ran_ugaussian(*_r);
  
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();
  
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t j = 0; j < _k; ++j) {
      assert(bd[j]);
      vd1[i][j] = ad[i][j] / bd[j];
      vd2[i][j] = gsl_sf_psi(ad[i][j]) - log(bd[j]);
    }
  set_to_prior();
  */
  
  double **ad = _scurr.data();
  double *bd = _rcurr.data();
  
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k) {
            ad[i][k] = _sprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
    }
  }
  
  for (uint32_t k = 0; k < _k; ++k) {
    bd[k] = _rprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
  }
  set_to_prior();
}

inline void
GPMatrixGR::initialize2(double v, double offset)
{
  double **ad = _scurr.data();
  double *bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i) {
    for (uint32_t k = 0; k < _k; ++k) {
      ad[i][k] = _sprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
    }
  }
  for (uint32_t k = 0; k < _k; ++k)
    bd[k] = _rprior + v;
  set_to_prior();
}


inline void
GPMatrixGR::initialize_exp(double v, double offset)
{
  double **ad = _scurr.data();
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  Array b(_k);  
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k) {
      b[k] = v + offset * 0.01 * gsl_ran_ugaussian(*_r);
      vd1[i][k] = ad[i][k] / b[k];
      vd2[i][k] = gsl_sf_psi(ad[i][k]) - log(b[k]);
    }
  set_to_prior();
} 


inline void
GPMatrixGR::initialize_exp(double offset)
{
  double **ad = _scurr.data();
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  Array b(_k);  
  for (uint32_t i = 0; i < _n; ++i)
    for (uint32_t k = 0; k < _k; ++k) {
      b[k] = _rprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
      vd1[i][k] = ad[i][k] / b[k];
      vd2[i][k] = gsl_sf_psi(ad[i][k]) - log(b[k]);
    }
  set_to_prior();
} 

inline double
GPMatrixGR::compute_elbo_term_helper() const
{
  const double **etheta = _Ev.const_data();
  const double **elogtheta = _Elogv.const_data();
  const double ** const ad = shape_curr().const_data();
  const double * const bd = rate_curr().const_data();

  double s = .0;
  for (uint32_t n = 0; n < _n; ++n)  {
    for (uint32_t k = 0; k < _k; ++k) {
      s += _sprior * log(_rprior) + (_sprior - 1) * elogtheta[n][k];
      s -= _rprior * etheta[n][k] + gsl_sf_lngamma(_sprior);
      debug("ehelper: %f:%f:%f log:%f\n", 
	    s, etheta[n][k], gsl_sf_lngamma(_sprior), elogtheta[n][k]);
    }
    double a = .0, b = .0;
    for (uint32_t k = 0; k < _k; ++k) {
      make_nonzero(ad[n][k], bd[k], a, b);
      s -= a * log(b) + (a - 1) * elogtheta[n][k];
      s += b * etheta[n][k] + gsl_sf_lngamma(a);
    }
  }
  return s;
}

// Saves the mean, shape, and rate parameters in the file. The argument should contain _ratings.seq2movie(), the mapping from indices in the program to the codes for items and users to print in the output
inline void
GPMatrixGR::save_state(const IDMap &m, string filename) const
{
  string expv_fname = string("/") + name() + ".tsv";
  string shape_fname = string("/") + name() + "_shape.tsv";
  string rate_fname = string("/") + name() + "_rate.tsv";
  _scurr.save(filename+"/"+Env::outfile_str(shape_fname), m);
  _rcurr.save(filename+"/"+Env::outfile_str(rate_fname), m);
  _Ev.save(filename+"/"+Env::outfile_str(expv_fname), m);
}

inline void
GPMatrixGR::load()
{
  string shape_fname = name() + "_shape.tsv";
  string rate_fname = name() + "_rate.tsv";
  _scurr.load(shape_fname);
  _rcurr.load(rate_fname);
  compute_expectations();
  lerr("loaded from %s and %s", 
       shape_fname.c_str(), rate_fname.c_str());
}

inline void
GPMatrixGR::load_from_lda(string dir, double alpha, uint32_t K)
{
  char buf[1024];
  sprintf(buf, "%s/lda-fits/%s-lda-k%d.tsv", dir.c_str(), name().c_str(), K);
  lerr("loading from %s", buf);
  _Ev.load(buf, 0, true);
  double **vd1 = _Ev.data();
  double **vd2 = _Elogv.data();

  for (uint32_t k = 0; k < _k; ++k) {
    double s = _Ev.colsum(k) + alpha * _n;
    for (uint32_t i = 0; i < _n; ++i) {
      double counts = vd1[i][k];
      vd1[i][k] = (alpha + counts) / s;
      vd2[i][k] = gsl_sf_psi(alpha + counts) - gsl_sf_psi(s);
    }
  }
  IDMap m;
  string expv_fname = string("/") + name() + ".tsv";
  _Ev.save(Env::file_str(expv_fname), m);
}

class GPArray : public GPBase<Array> {
public:
  GPArray(string name, 
	  double a, double b,
	  uint32_t n, gsl_rng **r): 
    GPBase<Array>(name),
    _n(n),
    _sprior(a), // shape 
    _rprior(b), // rate
    _scurr(n), _snext(n),
    _rnext(n), _rcurr(n),
    _Ev(n), _Elogv(n), _Einv(n),
    _r(r) { }
  ~GPArray() {}

  uint32_t n() const { return _n;}
  uint32_t k() const { return 0;}

  const Array &shape_curr() const         { return _scurr; }
  const Array &rate_curr() const          { return _rcurr; }
  const Array &shape_next() const         { return _snext; }
  const Array &rate_next() const          { return _rnext; }
  const Array &expected_v() const         { return _Ev;    }
  const Array &expected_logv() const      { return _Elogv; }
  const Array &expected_inv() const      { return _Einv; }
  
  Array &shape_curr()       { return _scurr; }
  Array &rate_curr()        { return _rcurr; }
  Array &shape_next()       { return _snext; }
  Array &rate_next()        { return _rnext; }
  Array &expected_v()       { return _Ev;    }
  Array &expected_logv()    { return _Elogv; }
  Array &expected_inv()    { return _Einv; }

  const double sprior() const { return _sprior; }
  const double rprior() const { return _rprior; }
  
  uint32_t n() { return _n; }

  void set_to_prior();
  void set_to_prior_curr();
  void update_shape_next(const Array &phi);
  void update_shape_next(uint32_t n, double v);
  void update_shape_next(double v);
  void update_rate_next(const Array &v);
  void update_rate_next(const Array &v, double scale);
  void update_rate_next(uint32_t n, double v);
  void swap();
  void compute_expectations();
  void initialize(double offset);
  void  initialize2(double v, double offset);
  void initialize_exp();

  double compute_elbo_term_helper() const;
  void save_state(const IDMap &m, string filename) const;
  void load();
  
  double expected_mean() const;

private:
  uint32_t _n;
  double _sprior;
  double _rprior;
  Array _scurr;      // current variational shape posterior 
  Array _snext;      // to help compute gradient update
  Array _rcurr;      // current variational rate posterior (global)
  Array _rnext;      // help compute gradient update
  Array _Ev;         // expected weights under variational
		     // distribution
  Array _Elogv;      // expected log weights
  Array _Einv;      // expected inverse weights
  gsl_rng **_r;
};

inline void
GPArray::set_to_prior()
{
  _snext.set_elements(_sprior);
  _rnext.set_elements(_rprior);
}

inline void
GPArray::update_shape_next(const Array &sphi)
{
  assert (sphi.size() == _n);
  _snext += sphi;
}

inline void
GPArray::update_shape_next(uint32_t n, double v)
{
  _snext[n] += v;
}

// Add v to the new shape parameter
inline void
GPArray::update_shape_next(double v)
{
  for (uint32_t n = 0; n < _n; ++n)
    _snext[n] += v;
}

// Adds v to _rnext
inline void
GPArray::update_rate_next(const Array &v)
{
  assert (v.size() == _n);
  _rnext += v;
}

// Adds a scaled up version of v to _rnext
inline void
GPArray::update_rate_next(const Array &v, double scale)
{
  assert (v.size() == _n);
  
  Array update(v);
  
  _rnext += update.scale(scale);
}

inline void
GPArray::update_rate_next(uint32_t n, double v)
{
  _rnext[n] += v;
}

inline void
GPArray::swap()
{
  _scurr.swap(_snext);
  _rcurr.swap(_rnext);
  set_to_prior();
}

inline void
GPArray::set_to_prior_curr()
{
  _scurr.set_elements(_sprior);
  _rcurr.set_elements(_rprior);
}

// Sets the means of user and item attributes and the log means ( used for the vector of parameters for the multinomial distribution, phi in the paper) for the first iteration, at the computed values from previous parameters plus a small shock
inline void
GPArray::compute_expectations()
{
  // Gets the matrices of current parameters which should have been initialized already
  const double * const ad = _scurr.const_data();
  const double * const bd = _rcurr.const_data();
  
  // Gets the matrices of means, log means, and inverse means to modify them
  double *vd1 = _Ev.data();
  double *vd2 = _Elogv.data();
  double *inv = _Einv.data();
  double a = .0, b = .0;
  for (uint32_t i = 0; i < _n; ++i) {
    
    // Sets the variables at a very small nonzero value
    make_nonzero(ad[i], bd[i], a, b);
    // Means: shape/rate parameter
    vd1[i] = a / b;
    // Log mean: From expectation of a Gamma r.v.
    vd2[i] = gsl_sf_psi(a) - log(b);
    // Mean of inverse: From expectation of an inverse Gamma r.v.
    inv[i] = b/(a-1);
  }
}

inline void
GPArray::initialize(double offset)
{
  double *ad = _scurr.data();
  double *bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i) {
    ad[i] = _sprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
    bd[i] = _rprior + offset * 0.01 * gsl_ran_ugaussian(*_r);
  }
  set_to_prior();
}

inline void
GPArray::initialize2(double v, double offset)
{
  double *ad = _scurr.data();
  double *bd = _rcurr.data();
  for (uint32_t i = 0; i < _n; ++i) {
    
    // -----------------------------------------
    // Error?????
//    ad[i] = _sprior + 0.01 * gsl_ran_ugaussian(*_r);
//    bd[i] = _rprior + v;
    // -----------------------------------------
    
    ad[i] = _sprior + v;
    bd[i] = _rprior * (1 + offset * 0.01 * gsl_ran_ugaussian(*_r));
  }
  set_to_prior();
}

inline double
GPArray::compute_elbo_term_helper() const
{
  const double *etheta = _Ev.const_data();
  const double *elogtheta = _Elogv.const_data();
  const double * const ad = shape_curr().const_data();
  const double * const bd = rate_curr().const_data();

  double s = .0;
  double a = .0, b = .0;
  for (uint32_t n = 0; n < _n; ++n)  {
    make_nonzero(ad[n], bd[n], a, b);
    s += _sprior * log(_rprior) + (_sprior - 1) * elogtheta[n];
    s -= _rprior * etheta[n] + gsl_sf_lngamma(_sprior);
    s -= a * log(b) + (a - 1) * elogtheta[n];
    s += b * etheta[n] + gsl_sf_lngamma(a);
  }
  return s;
}

inline void
GPArray::save_state(const IDMap &m, string filename) const
{
  string expv_fname = string("/") + name() + ".tsv";
  string shape_fname = string("/") + name() + "_shape.tsv";
  string rate_fname = string("/") + name() + "_rate.tsv";
  _scurr.save(filename+"/"+Env::outfile_str(shape_fname), m);
  _rcurr.save(filename+"/"+Env::outfile_str(rate_fname), m);
  _Ev.save(filename+"/"+Env::outfile_str(expv_fname), m);
}

inline void
GPArray::load()
{
  string shape_fname = name() + "_shape.tsv";
  string rate_fname = name() + "_rate.tsv";
  _scurr.load(shape_fname);
  _rcurr.load(rate_fname);
  compute_expectations();
}

// Saves the means of the expected values over users/items
inline double
GPArray::expected_mean() const {
  //  cout << means.size() << endl;
  //  cout << _k << endl;
  return _Ev.mean();
}

typedef D1Array<GPMatrix *> ItemMap;
#endif
