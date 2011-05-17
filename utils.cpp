/*
  This file is part of LOOS.

  LOOS (Lightweight Object-Oriented Structure library)
  Copyright (c) 2008, Tod D. Romo, Alan Grossfield
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



#include <sys/types.h>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <unistd.h>
#include <pwd.h>
#include <glob.h>

#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>

#include <boost/algorithm/string.hpp>

#include <Selectors.hpp>
#include <Parser.hpp>
#include <utils.hpp>




namespace loos {


  std::string findBaseName(const std::string& s) {
    std::string result;

    int n = s.find('.');
    result = (n <= 0) ? s : s.substr(0, n);
    
    return(result);
  }


  std::string getNextLine(std::istream& is, int *lineno = 0) {
    std::string s;
    std::string::size_type i;

    for (;;) {
      if (getline(is, s, '\n').eof())
        break;

      if (lineno != 0)
        *lineno += 1;

      // Strip off comments
      i = s.find('#');
      if (i != std::string::npos)
        s.erase(i, s.length() - i);

      // Remove leading whitespace
      i = s.find_first_not_of(" \t");
      if (i > 0)
        s.erase(0, i);

      // Is std::string non-empty?
      if (s.length() != 0)
        break;
    }

    return(s);
  }



  std::string invocationHeader(int argc, char *argv[]) {
    std::string invoke, user;
    int i;
  
    time_t t = time(0);
    std::string timestamp(asctime(localtime(&t)));
    timestamp.erase(timestamp.length() - 1, 1);

    struct passwd* pwd = getpwuid(getuid());
    if (pwd == 0)
      user = "UNKNOWN USER";
    else
      user = pwd->pw_name;

    invoke = std::string(argv[0]) + " ";
    std::string sep(" ");
    for (i=1; i<argc; i++) {
      if (i == argc-1)
        sep = "";
      invoke += "'" + std::string(argv[i]) + "'" + sep;
    }

    invoke += " - " + user + " (" + timestamp + ")";

#if defined(REVISION)
    invoke += " [" + std::string(REVISION) + "]";
#endif

    // Since some args my be brought in from a file via the shell
    // back-tick operator, we process embedded returns...
    boost::replace_all(invoke, "\n", "\\n");

    return(invoke);
  }



  GCoord boxFromRemarks(const Remarks& r) {
    int n = r.size();
    int i;

    GCoord c(99999.99, 99999.99, 99999.99);

    for (i=0; i<n; i++) {
      std::string s = r[i];
      if (s.substr(0, 6) == " XTAL ") {
        std::stringstream is(s.substr(5));
        if (!(is >> c.x()))
          throw(ParseError("Unable to parse " + s));
        if (!(is >> c.y()))
          throw(ParseError("Unable to parse " + s));
        if (!(is >> c.z()))
          throw(ParseError("Unable to parse " + s));

        break;
      }
    }

    return(c);
  }



  bool remarksHasBox(const Remarks& r) {
    int n = r.size();
    for (int i = 0; i<n; i++) {
      std::string s = r[i];
      if (s.substr(0, 6) == " XTAL ")
        return(true);
    }
    return(false);
  }



  base_generator_type& rng_singleton(void) {
    static base_generator_type rng;

    return(rng);
  }


  // Seeding based on the block is not the best method, but probably
  // sufficient for our purposes...
  uint randomSeedRNG(void) {
    base_generator_type& rng = rng_singleton();

    uint seedval = static_cast<uint>(time(0));

    rng.seed(seedval);
    return(seedval);
  }


  std::vector<int> parseRangeList(const std::string& text) {
    return(parseRangeList<int>(text));
  }

  /** This routine parses the passed string, turning it into a selector
   *  and applies it to \a source.  If there is an exception in the
   *  parsing, this is repackaged into a more sensible error message
   *  (including the string that generated the error).  No other
   *  exceptions are caught.
   *
   *  We're also assuming that you're <EM>always</EM> wanting to select
   *  some atoms, so lack of selection constitutes an error and an
   *  exception is thrown.  Note that in both the case of a parse error
   *  and null-selection, a runtime_error exception is thrown so the
   *  catcher cannot disambiguate between the two.
   */
  AtomicGroup selectAtoms(const AtomicGroup& source, const std::string selection) {
  
    Parser parser;

    try {
      parser.parse(selection);
    }
    catch(ParseError e) {
      throw(ParseError("Error in parsing '" + selection + "' ... " + e.what()));
    }

    KernelSelector selector(parser.kernel());
    AtomicGroup subset = source.select(selector);

    if (subset.size() == 0)
      throw(NullResult("No atoms were selected using '" + selection + "'"));

    return(subset);
  }

  std::string timeAsString(const double t) {
    if (t < 90.0) {
      std::stringstream s;
      s << std::fixed << std::setprecision(3) << t << "s";
      return(s.str());
    }
  
    double mins = floor(t / 60.0);
    double secs = t - mins * 60.0;
    if (mins < 90.0) {
      std::stringstream s;
      s << std::fixed << std::setprecision(0) << mins << "m" << std::setprecision(3) << secs << "s";
      return(s.str());
    }
  
    double hrs = floor(mins / 60.0);
    mins -= hrs * 60.0;
    std::stringstream s;
    s << std::fixed << std::setprecision(0) << hrs << "h" << mins << "m" << std::setprecision(3) << secs << "s";
    return(s.str());
  }


  template<>
  std::string parseStringAs<std::string>(const std::string& source, const uint pos, const uint nelem) {
    std::string val;

    uint n = !nelem ? source.size() - pos : nelem;
    if (pos + n > source.size())
      return(val);

    for (uint i=pos; i<pos+n; ++i)
      if (source[i] != ' ')
        val += source[i];

    return(val);
  }

  template<>
  std::string fixedSizeFormat(const std::string& s, const uint n) {
    uint m = s.size();
    if (m > n)
      return(s.substr(m-n, n));
    return(s);
  }


  namespace {
    int pow10[6]={1,10,100,1000,10000,100000};
    int pow36[6]={1,36,1296,46656,1679616,60466176};
  };


  
  int parseStringAsHybrid36(const std::string& source, const uint pos, const uint nelem) {
    uint n = !nelem ? source.size() - pos : nelem;
    if (pos + n > source.size())
      return(0);

    if (n > 6)
      throw(std::logic_error("Requested size exceeds max"));
    
    std::string s(source.substr(pos, n));
    bool negative(false);
    std::string::iterator si(s.begin());
    n = s.size();

    if (*si == '-') {
      negative = true;
      ++si;
      --n;
    }

    // Skip leading whitespace
    for (;si != s.end() && *si == ' '; ++si, --n) ;

    int offset = 0;   // This adjusts the range of the result
    char cbase = 'a'; // Which set or characters (upper or lower) for the alpha-part
    int ibase = 10;   // Number-base (i.e. 10 or 36)

    // Decide which chunk we're in...
    if (*si >= 'a') {
      offset = pow10[n] + 16*pow36[n-1];
      cbase = 'a';
      ibase = 36;
    } else if (*si >= 'A') {
      offset = pow10[n] - 10*pow36[n-1];
      cbase = 'A';
      ibase = 36;
    }

    int result = 0;
    while (si != s.end()) {
      int c = (*si >= cbase) ? *si-cbase+10 : *si-'0';
      result = result * ibase + c;
      ++si;
    }

    result += offset;

    if (negative)
      return(-result);

    return(result);
  }



  // Note: this currently will overflow if sufficiently negative
  // to overflow the base-10 part...
  std::string hybrid36AsString(int d, uint n) {

    if (n > 6)
      throw(std::logic_error("Requested size exceeds max"));

    int n10 = pow10[n];
    int n36 = pow36[n-1];
    int cuta = n10 + n36 * 26;  // Cutoff between upper and lower
                                // representations (i.e. A000 vs a000)
    bool negative(false);

    if (d < 0) {
      negative = true;
      d = -d;
    }

    if (d >= n10 + 52 * n36)
      throw(std::runtime_error("Number out of range"));

    unsigned char coffset = '0';   // Digits offset for output
    int ibase = 10;                // Numeric base (i.e. 10 or 36)

    if (d >= cuta) {
      coffset = 'a' - 10;
      ibase = 36;
      d -= cuta;
      d += 10*n36;
    } else if (d >= n10) {
      coffset = 'A' - 10;
      d -= n10;
      d += 10*n36;
      ibase = 36;
    }

    std::string result;
    while (d > 0) {
      unsigned char digit = d % ibase;
      digit += (digit > 9) ? coffset : '0';

      result.push_back(digit);
      d /= ibase;
    }

    if (negative)
      result.push_back('-');

    // right-justify...should we be left instead??
    for (uint i=result.size(); i < n; ++i)
      result.push_back(' ');

    std::reverse(result.begin(), result.end());
    return(result);
  }


  std::string anyToString(const boost::any& x) {
    std::string s;

    if (x.type() == typeid(int))
      s = boost::lexical_cast<std::string>(boost::any_cast<int>(x));
    else if (x.type() == typeid(double))
      s = boost::lexical_cast<std::string>(boost::any_cast<double>(x));
    else if (x.type() == typeid(std::string))
      s = boost::any_cast<std::string>(x);
    else if (x.type() == typeid(bool))
      s = (boost::any_cast<bool>(x)) ? "true" : "false";
    else if (x.type() == typeid(uint))
      s = boost::lexical_cast<std::string>(boost::any_cast<uint>(x));
    else if (x.type() == typeid(ulong))
      s = boost::lexical_cast<std::string>(boost::any_cast<ulong>(x));
    else if (x.type() == typeid(float))
      s = boost::lexical_cast<std::string>(boost::any_cast<float>(x));
    else if (x.type() == typeid(std::vector<std::string>))
      s = vToString( boost::any_cast< std::vector<std::string> >(x));
    else if (x.type() == typeid(std::vector<double>))
      s = vToString(boost::any_cast< std::vector<double> >(x));
    else if (x.type() == typeid(std::vector<uint>))
      s = vToString(boost::any_cast< std::vector<uint> >(x));
    else
      throw(LOOSError("Unknown type in anyToString() conversion"));

    return(s);

  }



  std::vector<std::string> optionsValues(const boost::program_options::variables_map& m) {
    std::vector<std::string> results;

    for (boost::program_options::variables_map::const_iterator i = m.begin(); i != m.end(); ++i) {
      std::ostringstream oss;
      oss << "# " << (*i).first << " = '" << anyToString((*i).second.value()) << "'";
      results.push_back(oss.str());
    }
    
    return(results);
  }


  std::string stringsAsComments(const std::vector<std::string>& v) {
    std::string s;

    for (std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i)
      s += "# " + *i + "\n";

    return(s);
  }


  std::string stringsAsString(const std::vector<std::string>& v) {
    std::string s;

    for (std::vector<std::string>::const_iterator i = v.begin(); i != v.end(); ++i)
      s += *i + "\n";

    // Remove the trailing newline...
    s.erase(s.end()-1);

    return(s);
  }


  AtomicGroup loadStructureWithCoords(const std::string& model_name, const std::string& coord_name) const {
      AtomicGroup model = createSystem(model_name);
      if (!coord_name.empty()) {
        AtomicGroup coords = createSystem(coord_name);
        model.copyCoordinates(coords);
      }

      if (! model.hasCoords())
        throw(LOOSError("Error- no coordinates found in specified model(s)"));
      
      return(model);
  }


  std::vector<uint> assignTrajectoryFrames(pTraj& traj, const std::string& frame_index_spec, uint skip = 0) const {
    std::vector<uint> frames;
    
    if (frame_index_spec.empty())
      for (uint i=skip; i<trajectory->nframes(); ++i)
        frames.push_back(i);
    else
      frames = parseRangeList<uint>(frame_index_spec);
    
    return(frames);
  }


}
