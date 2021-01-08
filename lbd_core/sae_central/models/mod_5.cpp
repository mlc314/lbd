// see fit_mod_5.r for model description

#include <TMB.hpp>
using namespace density;
using Eigen::SparseMatrix; 
#include "lcar_strmat.hpp"

// Objective function
template<class Type>
Type objective_function<Type>::operator() () {

// Define data and inputs
  DATA_VECTOR(Y);     // events
  DATA_VECTOR(N);     // population
  DATA_MATRIX(X);     // covariates (at minimum, a column for the intercept) 
  DATA_IVECTOR(J);    // area indicator
  DATA_IVECTOR(T);    // year indicator
  DATA_IVECTOR(A);    // age indicator
  
  DATA_SPARSE_MATRIX(graph_j);  // neighborhood structure
  DATA_SPARSE_MATRIX(graph_t); 
  DATA_SPARSE_MATRIX(graph_a); 

// Define parameters
  // fixed effects (intercept, covariate effects)
  PARAMETER_VECTOR(B);
  
  // RE1: age-year-level random intercept (LCAR:LCAR)
  PARAMETER_ARRAY(re1);
  PARAMETER(log_sigma2_1); 
  Type sigma_1 = exp(0.5*log_sigma2_1);
  Type prec_1 = exp(-1*log_sigma2_1); 
  PARAMETER(logit_rho_1a);
  Type rho_1a = invlogit(logit_rho_1a); 
  PARAMETER(logit_rho_1t);
  Type rho_1t = invlogit(logit_rho_1t);

  // RE2: area-year-level random intercept (LCAR:LCAR)
  PARAMETER_ARRAY(re2);
  PARAMETER(log_sigma2_2); 
  Type sigma_2 = exp(0.5*log_sigma2_2);
  Type prec_2 = exp(-1*log_sigma2_2); 
  PARAMETER(logit_rho_2j);
  Type rho_2j = invlogit(logit_rho_2j);
  PARAMETER(logit_rho_2t);
  Type rho_2t = invlogit(logit_rho_2t);
 
  // RE3: area-level random slope on age (LCAR)
  PARAMETER_VECTOR(re3); 
  PARAMETER(log_sigma2_3);
  Type sigma_3 = exp(0.5*log_sigma2_3);
  Type prec_3 = exp(-1*log_sigma2_3); 
  PARAMETER(logit_rho_3);
  Type rho_3 = invlogit(logit_rho_3);

// NLL contribution from random effects  
  Type nll = 0; 
  max_parallel_regions = omp_get_max_threads(); 
  
  // RE1: age-year-level random intercept
  SparseMatrix<Type> K_1a = lcar_strmat(graph_a, rho_1a);   
  SparseMatrix<Type> K_1t = lcar_strmat(graph_t, rho_1t);  
  PARALLEL_REGION nll += SCALE(SEPARABLE(GMRF(K_1t), GMRF(K_1a)), sigma_1)(re1); 

  // RE2: area-year-level random intercept
  SparseMatrix<Type> K_2j = lcar_strmat(graph_j, rho_2j);   
  SparseMatrix<Type> K_2t = lcar_strmat(graph_t, rho_2t);  
  PARALLEL_REGION nll += SCALE(SEPARABLE(GMRF(K_2t), GMRF(K_2j)), sigma_2)(re2); 

  // RE3: area-level random slope on age
  SparseMatrix<Type> K_3 = lcar_strmat(graph_j, rho_3); 
  PARALLEL_REGION nll += SCALE(GMRF(K_3), sigma_3)(re3); 
  
  // Hyperpriors
  PARALLEL_REGION nll -= dgamma(prec_1, Type(1), Type(1000), true);
  PARALLEL_REGION nll -= dgamma(prec_2, Type(1), Type(1000), true);
  PARALLEL_REGION nll -= dgamma(prec_3, Type(1), Type(1000), true);
  PARALLEL_REGION nll -= dnorm(logit_rho_1a, Type(0), Type(1.5), true);
  PARALLEL_REGION nll -= dnorm(logit_rho_1t, Type(0), Type(1.5), true);
  PARALLEL_REGION nll -= dnorm(logit_rho_2j, Type(0), Type(1.5), true);
  PARALLEL_REGION nll -= dnorm(logit_rho_2t, Type(0), Type(1.5), true);
  PARALLEL_REGION nll -= dnorm(logit_rho_3, Type(0), Type(1.5), true);
  
// NLL contribution from data 
  // predictions
  vector<Type> log_m = X * B; 
  for(size_t i = 0; i < Y.size(); i++)
    log_m[i] += re1(A[i], T[i]) + re2(J[i], T[i]) + re3[J[i]]*A[i];
  vector<Type> m = exp(log_m); 

  // data likelihood 
  for(size_t i = 0; i < Y.size(); i++)
    PARALLEL_REGION nll -= dpois(Y[i], N[i]*m[i], true); 
  
  return nll;
}
