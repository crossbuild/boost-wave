/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library
    http://www.boost.org/

    Copyright (c) 2001-2005 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_WAVE_LIBS_WAVE_TEST_CMD_LINE_UTILS_HPP)
#define BOOST_WAVE_LIBS_WAVE_TEST_CMD_LINE_UTILS_HPP

#include <string>
#include <fstream>

#include <boost/config.hpp>
#include <boost/assert.hpp>
#include <boost/program_options.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace cmd_line_utils {

    namespace po = boost::program_options;

    ///////////////////////////////////////////////////////////////////////////
    // Additional command line parser which interprets '@something' as an 
    // option "config-file" with the value "something".
    inline std::pair<std::string, std::string> 
    at_option_parser(std::string const& s)
    {
        if ('@' == s[0]) 
            return std::make_pair(std::string("config-file"), s.substr(1));
        else
            return std::pair<std::string, std::string>();
    }

    ///////////////////////////////////////////////////////////////////////////
    // class, which keeps the include file information read from the options
    class include_paths 
    {
    public:
        include_paths() : seen_separator(false) {}

        std::vector<std::string> paths;       // stores user paths
        std::vector<std::string> syspaths;    // stores system paths
        bool seen_separator;                  // options contain a '-I-' option

        // Function which validates additional tokens from the given option.
        static void 
        validate(boost::any &v, std::vector<std::string> const &tokens)
        {
            if (v.empty())
                v = boost::any(include_paths());

            include_paths *p = boost::any_cast<include_paths>(&v);
            BOOST_ASSERT(p);

        // Assume only one path per '-I' occurrence.
            std::string t = tokens[0];
            if (t == "-") {
            // found -I- option, so switch behaviour
                p->seen_separator = true;
            } 
            else if (p->seen_separator) {
            // store this path as a system path
                p->syspaths.push_back(t); 
            } 
            else {
            // store this path as an user path
                p->paths.push_back(t);
            }            
        }
    };

    ///////////////////////////////////////////////////////////////////////////
    // Read all options from a given config string, parse and add them to the
    // given variables_map
    inline void 
    read_config_options(std::string const &indata, 
        po::options_description const &desc, po::variables_map &vm)
    {
        std::istringstream istrm(indata);
        
        std::vector<std::string> options;
        std::string line;

        while (std::getline(istrm, line)) {
        // skip empty lines
            std::string::size_type pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos) 
                continue;

        // skip comment lines
            if ('#' != line[pos])
                options.push_back(line);
        }

        if (options.size() > 0) {
            po::store(po::command_line_parser(options).options(desc).run(), vm);
            po::notify(vm);
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Read all options from a given config file, parse and add them to the
    // given variables_map
    inline bool
    read_config_file(std::string const &filename, 
        po::options_description const &desc, po::variables_map &vm)
    {
        std::ifstream ifs(filename.c_str());

        if (!ifs.is_open()) {
            std::cerr 
                << "testwave: " << filename 
                << ": command line warning: config file not found"
                << std::endl;
            return false;
        }
        
        std::vector<std::string> options;
        std::string line;

        while (std::getline(ifs, line)) {
        // skip empty lines
            std::string::size_type pos = line.find_first_not_of(" \t");
            if (pos == std::string::npos) 
                continue;

        // skip comment lines
            if ('#' != line[pos])
                options.push_back(line);
        }

        if (options.size() > 0) {
        // treat positional arguments as --input parameters
            po::positional_options_description p;
            p.add("input", -1);

        // parse the vector of lines and store the results into the given
        // variables map
            po::store(po::command_line_parser(options)
                .options(desc).positional(p).run(), vm);
            po::notify(vm);
        }
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    // predicate to extract all positional arguments from the command line
    struct is_argument 
    {
        bool operator()(po::option const &opt)
        {
            return (opt.position_key == -1) ? true : false;
        }
    };

///////////////////////////////////////////////////////////////////////////////
}   // namespace cmd_line_utils

///////////////////////////////////////////////////////////////////////////////
//
//  Special validator overload, which allows to handle the -I- syntax for
//  switching the semantics of an -I option.
//
//  This must be injected into the boost::program_options namespace
//
///////////////////////////////////////////////////////////////////////////////
namespace boost { namespace program_options {

    inline void 
    validate(boost::any &v, std::vector<std::string> const &s,
        cmd_line_utils::include_paths *, int) 
    {
        cmd_line_utils::include_paths::validate(v, s);
    }

}}  // namespace boost::program_options

#endif // !defined(BOOST_WAVE_LIBS_WAVE_TEST_CMD_LINE_UTILS_HPP)