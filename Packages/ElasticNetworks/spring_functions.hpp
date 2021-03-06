/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2010 Tod D. Romo
  Department of Biochemistry and Biophysics
  School of Medicine & Dentistry, University of Rochester

  This package (LOOS) is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation under version 3 of the License.

  This package is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/


/** \addtogroup ENM
 *@{
 */

#if !defined(LOOS_SPRING_FUNCTIONS_HPP)
#define LOOS_SPRING_FUNCTIONS_HPP




#include <loos.hpp>


namespace ENM {

  // Exceptions classes (primarily for springFactory())
  //! Bad spring function was requested
  class BadSpringFunction : public std::runtime_error
  {
  public:
    BadSpringFunction(const std::string& s) : runtime_error(s) { }
  };

  //! Unspecified problem with parameters in SpringFunction
  class BadSpringParameter : public std::runtime_error
  {
  public:
    BadSpringParameter(const std::string& s) : runtime_error(s) { }
  };



  //! Interface for ENM spring functions
  /**
   *These classes define the various possible spring functions used in
   *creating the Hessian.  All derived from the SpringFunction base
   *class.  This class returns a 3x3 DoubleMatrix containing the spring
   *constants...
   *
   *The SpringFunction::constant() function takes the coords of the two
   *nodes plus their difference vector (since it'll almost always be
   *computed prior to calling SpringFunction, no sense in recomputing
   *it).
   *
   *
   * =Misc notes=
   *
   *setParams() takes a vector of doubles that represents the internal
   *"constants" for the spring functions.  It treats the vector as a
   *LIFO stack and picks off the ones it neds, returning the rest.
   */
  class SpringFunction {
  public:
    typedef std::vector<double>      Params;
  public:
    SpringFunction() : warned(false) { }
    virtual ~SpringFunction() { }

    //! Name for this particular spring function
    virtual std::string name() const =0;

    //! Sets the internal constants, returning the unused ones
    virtual Params setParams(const Params& konst) =0;

    //! Determines if the internal constants are "valid"
    virtual bool validParams() const =0;

    //! How many internal constants there are
    virtual uint paramSize() const =0;


  
    //! Actually compute the spring constant as a 3x3 matrix
    virtual loos::DoubleMatrix constant(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d)  =0;

  protected:

    //! Check for negative spring-constants
    /**
     * Issues a one-time warning if a negative spring constant is found
     */
    double checkConstant(double d) {
      if (d < 0.0) {
        if (!warned) {
          warned = true;
          std::cerr << "Warning- negative spring constants found in " << name() << ".  Setting to 0.\n";
        }
        d = 0.0;
      }

      return(d);
    }

  private:
    bool warned;
  };



  //! Spring functions that are uniform in all directions (ie return a single value)
  /** Many spring functions are actually uniform over the 3x3 matrix.
   *The UniformSpringFunction uses the NVI idiom
   *(http://www.gotw.ca/publications/mill18.htm) to allow subclasses to
   *only return a double value, which then gets copied into all
   *elements of the 3x3 matrix.
   *
   *Note: this means you override the constantImpl() implementation
   *function, NOT the public constant() function.
   */

  class UniformSpringFunction : public SpringFunction {
  public:

    loos::DoubleMatrix constant(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
      double k = checkConstant(constantImpl(u, v, d));
      loos::DoubleMatrix B(3, 3);
      for (uint i=0; i<9; ++i)
        B[i] = k;

      return(B);
    }

  private:

    //! Implementation of the spring constant calculation
    virtual double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) =0;
  };



  // The following are all uniform spring constants...


  //! Basic distance cutoff for "traditional" ENM
  /**
     \f{eqnarray*}
     r^{-2} & \mbox{for} & r \leq r_c \\
     0 & \mbox{for} & r > r_c\\
     \f}
     *Where r is the distance between nodes.
     */
  class DistanceCutoff : public UniformSpringFunction {
  public:
    DistanceCutoff(const double& r) : radius(r*r) { }
    DistanceCutoff() : radius(15.0*15.0) { }

    std::string name() const { return("DistanceCutoff"); }

    Params setParams(const Params& p) {
      if (p.size() < 1)
        throw(BadSpringParameter("Insufficient number of spring parameters"));

      Params q(p);
      radius = q.back();
      radius *= radius;
      q.pop_back();
      return(q);
    }

    bool validParams() const { return(radius > 0.0); }

    uint paramSize() const { return(1); }

    double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
      double s = d.length2();
      if (s <= radius)
        return(1./s);

      return(0.0);
    }

  private:
    double radius;
  };


  //! Distance weighting (i.e. \f$r^p\f$)
  class DistanceWeight : public UniformSpringFunction {
  public:
    DistanceWeight(const double p) : power(p) { }
    DistanceWeight() : power(-2.0) { }

    std::string name() const { return("DistanceWeight"); }


    Params setParams(const Params& p) {
      if (p.size() < 1)
        throw(BadSpringParameter("Insufficient number of spring parameters"));

      Params q(p);
      power = q.back();
      q.pop_back();
      return(q);
    }

    bool validParams() const { return(power < 0.0); }

    uint paramSize() const { return(1); }


    double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
      double s = d.length();
      return(pow(s, power));
    }

  private:
    double power;
  };




  //! Exponential distance weighting (i.e. \f$\exp(k r)\f$)
  class ExponentialDistance : public UniformSpringFunction {
  public:
    ExponentialDistance(const double s) : scale(s) { }
    ExponentialDistance() : scale(-1.5) { }

    std::string name() const { return("ExponentialDistance"); }

    Params setParams(const Params& p) {
      if (p.size() < 1)
        throw(BadSpringParameter("Insufficient number of spring parameters"));

      Params q(p);
      scale = q.back();
      q.pop_back();
      return(q);
    }

    bool validParams() const { return(scale != 0.0); }

    uint paramSize() const { return(1); }


    double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
      double s = d.length();
      return(exp(scale * s));
    }

  private:
    double scale;
  };





  //! HCA method (bimodal distance-based function)
  /**
   * See <a href="http://dx.doi.org/10.1016/S0301-0104(00)00222-6">Hinsen et al, Chem Phys (2000) 261:25-37</a>
   *
   * Note: The defaults are the original Hinsen constants...
   *
   *\f{eqnarray*}
   ar + b & \mbox{for} & r < r_c \\
   c r^{-d} & \mbox{for} & r \geq r_c\\
   \f}
  */

  class HCA : public UniformSpringFunction {
  public:
    HCA(const double rc, const double a, const double b, const double c, const double d) :
      rcut(rc), k1(a), k2(b), k3(c), k4(d) { }

    HCA() :
      rcut(4.0), k1(205.5), k2(571.2), k3(305.9e3), k4(6.0) { }

    std::string name() const { return("HCA"); }

    Params setParams(const Params& p) {
      if (p.size() < 5)
        throw(BadSpringParameter("Insufficient number of spring parameters"));

      Params q(p);
    
      k4 = q.back();
      q.pop_back();

      k3 = q.back();
      q.pop_back();

      k2 = q.back();
      q.pop_back();

      k1 = q.back();
      q.pop_back();

      rcut = q.back();
      q.pop_back();

      return(q);
    }

    bool validParams() const { return(rcut >= 0.0 && k4 >= 0.0); }

    uint paramSize() const { return(5); }


    double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
      double s = d.length();
      double k;

      if (s <= rcut)
        k = k1 * s - k2;
      else
        k = k3 * pow(s, -k4);

      return(k);
    }

  private:
    double rcut, k1, k2, k3, k4;
  };

//! Use a spring function that is a constant weight regardless of distance
class ConstBonded : public UniformSpringFunction {
public:
  ConstBonded(const double& s) : scale(s) { }
  ConstBonded() : scale(1) { }
  
  std::string name() const { return("ConstBonded"); }

  Params setParams(const Params& p) {
    if (p.size() < 1)
      throw(BadSpringParameter("Insufficient number of spring parameters"));

    Params q(p);
    scale = q.back();
    q.pop_back();
    return(q);
  }

  bool validParams() const { return(scale > 0.0); }

  uint paramSize() const { return(1); }

  double constantImpl(const loos::GCoord& u, const loos::GCoord& v, const loos::GCoord& d) {
    //std::cerr << "In impl in constbonded :)\n";
    return(scale);
  }

private:
  double scale;
};

  //! Factory function for generating new SpringFunction instances based on a user string
  SpringFunction* springFactory(const std::string& spring_desc);

  //! List of possible names for springFactory()
  std::vector<std::string> springNames();

};

#endif

/** @} */
