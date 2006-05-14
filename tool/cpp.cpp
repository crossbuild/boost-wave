/*=============================================================================
    Boost.Wave: A Standard compliant C++ preprocessor library

    http://www.boost.org/

    Copyright (c) 2001-2006 Hartmut Kaiser. Distributed under the Boost
    Software License, Version 1.0. (See accompanying file
    LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#define BOOST_WAVE_SERIALIZATION        1             // enable serialization
#define BOOST_WAVE_BINARY_SERIALIZATION 1             // use binary archives

#include "cpp.hpp"                                    // global configuration

///////////////////////////////////////////////////////////////////////////////
// Include additional Boost libraries
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/timer.hpp>
#include <boost/any.hpp>

///////////////////////////////////////////////////////////////////////////////
//  Include Wave itself
#include <boost/wave.hpp>

///////////////////////////////////////////////////////////////////////////////
//  Include the lexer related stuff
#include <boost/wave/cpplexer/cpp_lex_token.hpp>      // token type
#include <boost/wave/cpplexer/cpp_lex_iterator.hpp>   // lexer type

///////////////////////////////////////////////////////////////////////////////
//  Include serialization support, if requested
#if BOOST_WAVE_SERIALIZATION != 0
#include <boost/serialization/serialization.hpp>
#if BOOST_WAVE_BINARY_SERIALIZATION != 0
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
typedef boost::archive::binary_iarchive iarchive;
typedef boost::archive::binary_oarchive oarchive;
#else
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
typedef boost::archive::text_iarchive iarchive;
typedef boost::archive::text_oarchive oarchive;
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
//  Include the context policies to use
#include "trace_macro_expansion.hpp"

///////////////////////////////////////////////////////////////////////////////
//  Include lexer specifics, import lexer names
#if BOOST_WAVE_SEPARATE_LEXER_INSTANTIATION == 0
#include <boost/wave/cpplexer/re2clex/cpp_re2c_lexer.hpp>
#endif 

///////////////////////////////////////////////////////////////////////////////
//  Include the grammar definitions, if these shouldn't be compiled separately
//  (ATTENTION: _very_ large compilation times!)
#if BOOST_WAVE_SEPARATE_GRAMMAR_INSTANTIATION == 0
#include <boost/wave/grammars/cpp_intlit_grammar.hpp>
#include <boost/wave/grammars/cpp_chlit_grammar.hpp>
#include <boost/wave/grammars/cpp_grammar.hpp>
#include <boost/wave/grammars/cpp_expression_grammar.hpp>
#include <boost/wave/grammars/cpp_predef_macros_grammar.hpp>
#include <boost/wave/grammars/cpp_defined_grammar.hpp>
#endif 

///////////////////////////////////////////////////////////////////////////////
//  Import required names
using namespace boost::spirit;

using std::string;
using std::pair;
using std::vector;
using std::getline;
using std::ifstream;
using std::ofstream;
using std::cout;
using std::cerr;
using std::endl;
using std::ostream;
using std::istreambuf_iterator;

///////////////////////////////////////////////////////////////////////////////
//
//  This application uses the lex_iterator and lex_token types predefined 
//  with the Wave library, but it is possible to use your own types.
//
//  You may want to have a look at the other samples to see how this is
//  possible to achieve.
    typedef boost::wave::cpplexer::lex_token<> token_type;
    typedef boost::wave::cpplexer::lex_iterator<token_type>
        lex_iterator_type;

//  The C++ preprocessor iterators shouldn't be constructed directly. They 
//  are to be generated through a boost::wave::context<> object. This 
//  boost::wave::context object is additionally to be used to initialize and 
//  define different parameters of the actual preprocessing.
    typedef boost::wave::context<
            std::string::iterator, lex_iterator_type,
            boost::wave::iteration_context_policies::load_file_to_string,
            trace_macro_expansion<token_type> > 
        context_type;

///////////////////////////////////////////////////////////////////////////////
// print the current version
string get_version()
{
    string version (context_type::get_version_string());
    version = version.substr(1, version.size()-2);      // strip quotes
    version += string(" (" CPP_VERSION_DATE_STR ")");   // add date
    return version;
}

///////////////////////////////////////////////////////////////////////////////
// print the current version for interactive sessions
int print_interactive_version()
{
    cout << "Wave: A Standard conformant C++ preprocessor based on the Boost.Wave library" << endl;
    cout << "Version: " << get_version() << endl;
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// print the copyright statement
int print_copyright()
{
    char const *copyright[] = {
        "",
        "Wave: A Standard conformant C++ preprocessor based on the Boost.Wave library",
        "http://www.boost.org/",
        "",
        "Copyright (c) 2001-2006 Hartmut Kaiser, Distributed under the Boost",
        "Software License, Version 1.0. (See accompanying file",
        "LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)",
        0
    };
    
    for (int i = 0; 0 != copyright[i]; ++i)
        cout << copyright[i] << endl;
        
    return 0;                       // exit app
}

///////////////////////////////////////////////////////////////////////////////
// forward declarations only
namespace cmd_line_utils 
{
    class include_paths;
}

namespace boost { namespace program_options {

    void validate(boost::any &v, std::vector<std::string> const &s,
        cmd_line_utils::include_paths *, long);

}} // boost::program_options

///////////////////////////////////////////////////////////////////////////////
#include <boost/program_options.hpp>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

///////////////////////////////////////////////////////////////////////////////
namespace cmd_line_utils {

    // Additional command line parser which interprets '@something' as an 
    // option "config-file" with the value "something".
    inline pair<string, string> 
    at_option_parser(string const&s)
    {
        if ('@' == s[0]) 
            return std::make_pair(string("config-file"), s.substr(1));
        else
            return pair<string, string>();
    }

    // class, which keeps include file information read from the command line
    class include_paths {
    public:
        include_paths() : seen_separator(false) {}

        vector<string> paths;       // stores user paths
        vector<string> syspaths;    // stores system paths
        bool seen_separator;        // command line contains a '-I-' option

        // Function which validates additional tokens from command line.
        static void 
        validate(boost::any &v, vector<string> const &tokens)
        {
            if (v.empty())
                v = boost::any(include_paths());

            include_paths *p = boost::any_cast<include_paths>(&v);

            BOOST_ASSERT(p);
            // Assume only one path per '-I' occurrence.
            string const& t = po::validators::get_single_string(tokens);
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

    // Read all options from a given config file, parse and add them to the
    // given variables_map
    void read_config_file_options(string const &filename, 
        po::options_description const &desc, po::variables_map &vm,
        bool may_fail = false)
    {
    ifstream ifs(filename.c_str());

        if (!ifs.is_open()) {
            if (!may_fail) {
                cerr << filename 
                    << ": command line warning: config file not found"
                    << endl;
            }
            return;
        }
        
    vector<string> options;
    string line;

        while (std::getline(ifs, line)) {
        // skip empty lines
            string::size_type pos = line.find_first_not_of(" \t");
            if (pos == string::npos) 
                continue;

        // skip comment lines
            if ('#' != line[pos])
                options.push_back(line);
        }

        if (options.size() > 0) {
            using namespace boost::program_options::command_line_style;
            po::store(po::command_line_parser(options)
                .options(desc).style(unix_style).run(), vm);
            po::notify(vm);
        }
    }

    // predicate to extract all positional arguments from the command line
    struct is_argument {
        bool operator()(po::option const &opt)
        {
          return (opt.position_key == -1) ? true : false;
        }
    };

///////////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
//
//  Special validator overload, which allows to handle the -I- syntax for
//  switching the semantics of an -I option.
//
///////////////////////////////////////////////////////////////////////////////
namespace boost { namespace program_options {

    void validate(boost::any &v, std::vector<std::string> const &s,
        cmd_line_utils::include_paths *, long) 
    {
        cmd_line_utils::include_paths::validate(v, s);
    }

}}  // namespace boost::program_options

///////////////////////////////////////////////////////////////////////////////
namespace {

    class auto_stop_watch : public stop_watch
    {
    public:
        auto_stop_watch(std::ostream &outstrm_)
        :   print_time(false), outstrm(outstrm_)
        {
        }
        
        ~auto_stop_watch()
        {
            if (print_time) {
                outstrm << "Elapsed time: " 
                      << this->format_elapsed_time() 
                      << std::endl;
            }
        }
    
        void set_print_time(bool print_time_)
        {
            print_time = print_time_;
        }
        
    private:
        bool print_time;
        std::ostream &outstrm;
    };

    ///////////////////////////////////////////////////////////////////////////
    inline std::string 
    report_iostate_error(std::ios::iostate state)
    {
        BOOST_ASSERT(state & (std::ios::badbit | std::ios::failbit | std::ios::eofbit));
        std::string result;
        if (state & std::ios::badbit) {
            result += "      the reported problem was: "
                      "loss of integrity of the stream buffer\n";
        }
        if (state & std::ios::failbit) {
            result += "      the reported problem was: "
                      "an operation was not processed correctly\n";
        }
        if (state & std::ios::eofbit) {
            result += "      the reported problem was: "
                      "end-of-file while writing to the stream\n";
        }
        return result;
    }                            

    ///////////////////////////////////////////////////////////////////////////
    //  Retrieve the position of a macro definition
    template <typename Context>
    inline bool
    get_macro_position(Context &ctx, 
        typename Context::token_type::string_type const& name,
        typename Context::position_type &pos)
    {
        bool has_parameters = false;
        bool is_predefined = false;
        std::vector<typename Context::token_type> parameters;
        typename Context::token_sequence_type definition;
        
        return ctx.get_macro_definition(name, has_parameters, is_predefined, 
            pos, parameters, definition);
    }

    ///////////////////////////////////////////////////////////////////////////
    //  Generate some meaningful error messages
    template <typename Context>
    inline int 
    report_error_message(Context &ctx, boost::wave::cpp_exception const &e)
    {
        // default error reporting
        cerr 
            << e.file_name() << ":" << e.line_no() << ":" << e.column_no() 
            << ": " << e.description() << endl;

        using boost::wave::preprocess_exception;
        switch(e.get_errorcode()) {
        case preprocess_exception::macro_redefinition:
            {
                // report the point of the initial macro definition
                typename Context::position_type pos;
                if (get_macro_position(ctx, e.get_related_name(), pos)) {
                    cerr 
                        << pos << ": " 
                        << preprocess_exception::severity_text(e.get_severity())
                        << ": this is the location of the previous definition." 
                        << endl;
                }
                else {
                    cerr 
                        << e.file_name() << ":" << e.line_no() << ":" 
                        << e.column_no() << ": " 
                        << preprocess_exception::severity_text(e.get_severity())
                        << ": not able to retrieve the location of the previous "
                        << "definition." << endl;
                }
            }
            break;

        default:
            break;
        }
        
        // errors count as one
        return (e.get_severity() == boost::wave::util::severity_error ||
                e.get_severity() == boost::wave::util::severity_fatal) ? 1 : 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  Read one logical line of text
    inline bool 
    read_a_line (std::istream &instream, std::string &instring)
    {
        bool eol = true;
        do {
            std::string line;
            std::getline(instream, line);
            if (instream.rdstate() & std::ios::failbit)
                return false;       // nothing to do

            eol = true;
            if (line.find_last_of('\\') == line.size()-1) 
                eol = false;

            instring += line + '\n';
        } while (!eol);
        return true;
    }

    ///////////////////////////////////////////////////////////////////////////
    //  Load and save the internal tables of the wave::context object
    template <typename Context>
    inline void 
    load_state(po::variables_map const &vm, Context &ctx)
    {
#if BOOST_WAVE_SERIALIZATION != 0
        try {
            if (vm.count("state") > 0) {
                fs::path state_file (vm["state"].as<string>(), fs::native);
                if (state_file == "-") 
                    state_file = fs::path("wave.state", fs::native);

                std::ios::openmode mode = std::ios::in;

#if BOOST_WAVE_BINARY_SERIALIZATION != 0
                mode = (std::ios::openmode)(mode | std::ios::binary);
#endif
                ifstream ifs (state_file.string().c_str(), mode);
                if (ifs.is_open()) {
                    iarchive ia(ifs);
                    string version;
                    
                    ia >> version;      // load version
                    if (version == CPP_VERSION_FULL_STR)
                        ia >> ctx;      // load the internal tables from disc
                    else {
                        cerr << "wave: detected version mismatch while loading state, state was not loaded." << endl;
                        cerr << "      loaded version:   " << version << endl;
                        cerr << "      expected version: " << CPP_VERSION_FULL_STR << endl;
                    }
                }
            }
        }
        catch (boost::archive::archive_exception const& e) {
            cerr << "wave: error while loading state: " 
                 << e.what() << endl;
        }
        catch (boost::wave::preprocess_exception const& e) {
            cerr << "wave: error while loading state: " 
                 << e.description() << endl;
        }
#endif
    }

    template <typename Context>
    inline void 
    save_state(po::variables_map const &vm, Context const &ctx)
    {
#if BOOST_WAVE_SERIALIZATION != 0
        try {
            if (vm.count("state") > 0) {
                fs::path state_file (vm["state"].as<string>(), fs::native);
                if (state_file == "-") 
                    state_file = fs::path("wave.state", fs::native);

                std::ios::openmode mode = std::ios::out;

#if BOOST_WAVE_BINARY_SERIALIZATION != 0
                mode = (std::ios::openmode)(mode | std::ios::binary);
#endif
                ofstream ofs(state_file.string().c_str(), mode);
                if (!ofs.is_open()) {
                    cerr << "wave: could not open state file for writing: " 
                         << state_file.string() << endl;
                    // this is non-fatal
                }
                else {
                    oarchive oa(ofs);
                    string version(CPP_VERSION_FULL_STR);
                    oa << version; // write version
                    oa << ctx;                  // write the internal tables to disc
                }
            }
        }
        catch (boost::archive::archive_exception const& e) {
            cerr << "wave: error while writing state: " 
                 << e.what() << endl;
        }
#endif
    }

///////////////////////////////////////////////////////////////////////////////
}   // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
//  do the actual preprocessing
int 
do_actual_work (std::string file_name, std::istream &instream, 
    po::variables_map const &vm, bool input_is_stdin)
{
// current file position is saved for exception handling
boost::wave::util::file_position_type current_position;
auto_stop_watch elapsed_time(cerr);
int error_count = 0;

    try {
    // process the given file
    string instring;

        instream.unsetf(std::ios::skipws);

        if (!input_is_stdin) {
#if defined(BOOST_NO_TEMPLATED_ITERATOR_CONSTRUCTORS)
            // this is known to be very slow for large files on some systems
            copy (istream_iterator<char>(instream),
                  istream_iterator<char>(), 
                  inserter(instring, instring.end()));
#else
            instring = string(istreambuf_iterator<char>(instream.rdbuf()),
                              istreambuf_iterator<char>());
#endif 
        }
        
    // The preprocessing of the input stream is done on the fly behind the 
    // scenes during iteration over the context_type::iterator_type stream.
    std::ofstream output;
    std::ofstream traceout;
    std::ofstream includelistout;
    
    trace_flags enable_trace = trace_nothing;
    
        if (vm.count("traceto")) {
        // try to open the file, where to put the trace output
        fs::path trace_file (vm["traceto"].as<string>(), fs::native);
        
            if (trace_file != "-") {
                fs::create_directories(trace_file.branch_path());
                traceout.open(trace_file.string().c_str());
                if (!traceout.is_open()) {
                    cerr << "wave: could not open trace file: " << trace_file 
                         << endl;
                    return -1;
                }
            }
            enable_trace = trace_macros;
        }
        if ((enable_trace & trace_macros) && !traceout.is_open()) {
        // by default trace to std::cerr
            traceout.copyfmt(cerr);
            traceout.clear(cerr.rdstate());
            static_cast<std::basic_ios<char> &>(traceout).rdbuf(cerr.rdbuf());
        }

    // Open the stream where to output the list of included file names
        if (vm.count("listincludes")) {
        // try to open the file, where to put the include list 
        fs::path includes_file(vm["listincludes"].as<string>(), fs::native);
        
            if (includes_file != "-") {
                fs::create_directories(includes_file.branch_path());
                includelistout.open(includes_file.string().c_str());
                if (!includelistout.is_open()) {
                    cerr << "wave: could not open include list file: " 
                         << includes_file.string() << endl;
                    return -1;
                }
            }
            enable_trace = trace_includes;
        }
        if ((enable_trace & trace_includes) && !includelistout.is_open()) {
        // by default list included names to std::cout
            includelistout.copyfmt(cout);
            includelistout.clear(cout.rdstate());
            static_cast<std::basic_ios<char> &>(includelistout).
                rdbuf(cout.rdbuf());
        }
        
    // enable preserving comments mode
    bool preserve_comments = false;
    bool preserve_whitespace = false;
    
        if (vm.count("preserve")) {
        int preserve = vm["preserve"].as<int>();
        
            switch(preserve) {
            case 0:   break;
            case 2:
                preserve_whitespace = true;
                /* fall through */
            case 1:
                preserve_comments = true;
                break;
                
            default:
                cerr << "wave: bogus preserve whitespace option value: " 
                     << preserve << ", should be 0, 1, or 2" << endl;
                return -1;
            }
        }

    // Since the #pragma wave system() directive may cause a potential security 
    // threat, it has to be enabled explicitly by --extended or -x
    bool enable_system_command = false;
    
        if (vm.count("extended")) 
            enable_system_command = true;

    // This this the central piece of the Wave library, it provides you with 
    // the iterators to get the preprocessed tokens and allows to configure
    // the preprocessing stage in advance.
    bool allow_output = true;   // will be manipulated from inside the hooks object
    string default_outfile;     // will be used from inside the hooks object
    trace_macro_expansion<token_type> hooks(preserve_whitespace, 
        output, traceout, includelistout, enable_trace, enable_system_command,
        allow_output, default_outfile);
    context_type ctx (instring.begin(), instring.end(), file_name.c_str(), hooks);

#if BOOST_WAVE_SUPPORT_VARIADICS_PLACEMARKERS != 0
    // enable C99 mode, if appropriate (implies variadics)
        if (vm.count("c99")) {
            ctx.set_language(
                boost::wave::language_support(
                    boost::wave::support_c99 
                 |  boost::wave::support_option_convert_trigraphs 
                 |  boost::wave::support_option_emit_line_directives 
#if BOOST_WAVE_SUPPORT_PRAGMA_ONCE != 0
                 |  boost::wave::support_option_include_guard_detection
#endif
                ));
        }
        else if (vm.count("variadics")) {
        // enable variadics and placemarkers, if appropriate
            ctx.set_language(boost::wave::enable_variadics(ctx.get_language()));
        }
#endif // BOOST_WAVE_SUPPORT_VARIADICS_PLACEMARKERS != 0

    // enable long long support, if appropriate
        if (vm.count("long_long")) {
            ctx.set_language(
                boost::wave::enable_long_long(ctx.get_language()));
        }

#if BOOST_WAVE_SUPPORT_PRAGMA_ONCE != 0
// disable include guard detection
        if (vm.count("noguard")) {
            ctx.set_language(
                boost::wave::enable_include_guard_detection(
                    ctx.get_language(), false));
        }
#endif
        
    // enable preserving comments mode
        if (preserve_comments) {
            ctx.set_language(
                boost::wave::enable_preserve_comments(ctx.get_language()));
        }
        
    // control the generation of #line directives
        if (vm.count("line")) {
            ctx.set_language(
                boost::wave::enable_emit_line_directives(ctx.get_language(), 
                    vm["line"].as<int>() != 0));
        }

    // add include directories to the system include search paths
        if (vm.count("sysinclude")) {
        vector<string> syspaths = vm["sysinclude"].as<vector<string> >();
        
            vector<string>::const_iterator end = syspaths.end();
            for (vector<string>::const_iterator cit = syspaths.begin(); 
                 cit != end; ++cit)
            {
                ctx.add_sysinclude_path((*cit).c_str());
            }
        }
        
    // add include directories to the include search paths
        if (vm.count("include")) {
            cmd_line_utils::include_paths const &ip = 
                vm["include"].as<cmd_line_utils::include_paths>();
            vector<string>::const_iterator end = ip.paths.end();

            for (vector<string>::const_iterator cit = ip.paths.begin(); 
                 cit != end; ++cit)
            {
                ctx.add_include_path((*cit).c_str());
            }

        // if -I- was goven on the command line, this has to be propagated
            if (ip.seen_separator) 
                ctx.set_sysinclude_delimiter();
                 
        // add system include directories to the include path
            vector<string>::const_iterator sysend = ip.syspaths.end();
            for (vector<string>::const_iterator syscit = ip.syspaths.begin(); 
                 syscit != sysend; ++syscit)
            {
                ctx.add_sysinclude_path((*syscit).c_str());
            }
        }
    
    // add additional defined macros 
        if (vm.count("define")) {
            vector<string> const &macros = vm["define"].as<vector<string> >();
            vector<string>::const_iterator end = macros.end();
            for (vector<string>::const_iterator cit = macros.begin(); 
                 cit != end; ++cit)
            {
                ctx.add_macro_definition(*cit);
            }
        }

    // add additional predefined macros 
        if (vm.count("predefine")) {
            vector<string> const &predefmacros = 
                vm["predefine"].as<vector<string> >();
            vector<string>::const_iterator end = predefmacros.end();
            for (vector<string>::const_iterator cit = predefmacros.begin(); 
                 cit != end; ++cit)
            {
                ctx.add_macro_definition(*cit, true);
            }
        }

    // undefine specified macros
        if (vm.count("undefine")) {
            vector<string> const &undefmacros = 
                vm["undefine"].as<vector<string> >();
            vector<string>::const_iterator end = undefmacros.end();
            for (vector<string>::const_iterator cit = undefmacros.begin(); 
                 cit != end; ++cit)
            {
                ctx.remove_macro_definition((*cit).c_str(), true);
            }
        }

    // maximal include nesting depth
        if (vm.count("nesting")) {
            int max_depth = vm["nesting"].as<int>();
            if (max_depth < 1 || max_depth > 100000) {
                cerr << "wave: bogus maximal include nesting depth: " 
                    << max_depth << endl;
                return -1;
            }
            ctx.set_max_include_nesting_depth(max_depth);
        }
        
    // open the output file
        if (vm.count("output")) {
        // try to open the file, where to put the preprocessed output
        fs::path out_file (vm["output"].as<string>(), fs::native);

            if (out_file == "-") {
                allow_output = false;     // inhibit output initially
                default_outfile = "-";
            }
            else {
                fs::create_directories(out_file.branch_path());
                output.open(out_file.string().c_str());
                if (!output.is_open()) {
                    cerr << "wave: could not open output file: " 
                         << out_file.string() << endl;
                    return -1;
                }
                default_outfile = out_file.string();
            }
        }
        else if (!input_is_stdin && vm.count("autooutput")) {
        // generate output in the file <input_base_name>.i
        fs::path out_file (file_name, fs::native);
        std::string basename (out_file.leaf());
        std::string::size_type pos = basename.find_last_of(".");
        
            if (std::string::npos != pos)
                basename = basename.substr(0, pos);
            out_file = out_file.branch_path() / (basename + ".i");

            fs::create_directories(out_file.branch_path());
            output.open(out_file.string().c_str());
            if (!output.is_open()) {
                cerr << "wave: could not open output file: " 
                     << out_file.string() << endl;
                return -1;
            }
            default_outfile = out_file.string();
        }

    // analyze the input file
    context_type::iterator_type first = ctx.begin();
    context_type::iterator_type last = ctx.end();
    
    // preprocess the required include files 
        if (vm.count("forceinclude")) {
        // add the filenames to force as include files in _reverse_ order
        // the second parameter 'is_last' of the force_include function should
        // be set to true for the last (first given) file.
            vector<string> const &force = 
                vm["forceinclude"].as<vector<string> >();
            vector<string>::const_reverse_iterator rend = force.rend();
            for (vector<string>::const_reverse_iterator cit = force.rbegin(); 
                 cit != rend; /**/)
            {
                string filename(*cit);
                first.force_include(filename.c_str(), ++cit == rend);
            }
        }

    //  we assume the session to be interactive if input is stdin and output is 
    //  stdout and the output is not inhibited
    bool is_interactive = input_is_stdin && !output.is_open() && allow_output;
    
        elapsed_time.set_print_time(!input_is_stdin && vm.count("timer") > 0);
        if (is_interactive) {
            print_interactive_version();  // print welcome message
            load_state(vm, ctx);          // load the internal tables from disc
        }
        else if (vm.count("state")) {
        // the option "state" is usable in interactive mode only
            cerr << "wave: ignoring the command line option 'state', "
                 << "use it in interactive mode only." << endl;
        }
        
    // >>>>>>>>>>>>> The actual preprocessing happens here. <<<<<<<<<<<<<<<<<<<
    // loop over the input lines if reading from stdin, otherwise this loop
    // will be executed once
        do {
        // loop over all generated tokens outputting the generated text 
        bool finished = false;
        
            if (input_is_stdin) {
                if (is_interactive) 
                    cout << ">>> ";     // prompt if is interactive

            // read next line and continue
                instring.clear();
                if (!read_a_line(instream, instring))
                    break;        // end of input reached
                first = ctx.begin(instring.begin(), instring.end());
            }
            
            do {
                try {
                    while (first != last) {
                    // store the last known good token position
                        current_position = (*first).get_position();

                    // print out the current token value
                        if (allow_output) {
                            if (output.rdstate() & (std::ios::badbit | std::ios::failbit | std::ios::eofbit))
                            {
                                cerr << "wave: problem writing to the current "
                                     << "output file" << endl;
                                cerr << report_iostate_error(output.rdstate());
                                break;
                            }
                            if (output.is_open())
                                output << (*first).get_value();
                            else
                                cout << (*first).get_value();
                        }
                        
                    // advance to the next token
                        ++first;
                    }
                    finished = true;
                }
                catch (boost::wave::cpp_exception const &e) {
                // some preprocessing error
                    if (is_interactive || boost::wave::is_recoverable(e)) {
                        error_count += report_error_message(ctx, e);
                    }
                    else {
                        throw;      // re-throw for non-recoverable errors
                    }
                }
            } while (!finished);
        } while (input_is_stdin);

        if (is_interactive) 
            save_state(vm, ctx);    // write the internal tables to disc
    }
    catch (boost::wave::cpp_exception const &e) {
    // some preprocessing error
        cerr 
            << e.file_name() << ":" << e.line_no() << ":" << e.column_no() << ": "
            << e.description() << endl;
        return 1;
    }
    catch (boost::wave::cpplexer::lexing_exception const &e) {
    // some lexing error
        cerr 
            << e.file_name() << ":" << e.line_no() << ":" << e.column_no() << ": "
            << e.description() << endl;
        return 2;
    }
    catch (std::exception const &e) {
    // use last recognized token to retrieve the error position
        cerr 
            << current_position << ": "
            << "exception caught: " << e.what()
            << endl;
        return 3;
    }
    catch (...) {
    // use last recognized token to retrieve the error position
        cerr 
            << current_position << ": "
            << "unexpected exception caught." << endl;
        return 4;
    }
    return -error_count;  // returns the number of errors as a negative integer
}

///////////////////////////////////////////////////////////////////////////////
//  main entry point
int
main (int argc, char *argv[])
{
    // test Wave compilation configuration
    if (!BOOST_WAVE_TEST_CONFIGURATION()) {
        cout << "wave: warning: the library this application was linked against was compiled "
             << endl 
             << "               using a different configuration (see wave_config.hpp)."
             << endl;
    }
    
    // analyze the command line options and arguments
    try {
    // declare the options allowed on the command line only
    po::options_description desc_cmdline ("Options allowed on the command line only");
        
        desc_cmdline.add_options()
            ("help,h", "print out program usage (this message)")
            ("version,v", "print the version number")
            ("copyright,c", "print out the copyright statement")
            ("config-file", po::value<vector<string> >()->composing(), 
                "specify a config file (alternatively: @filepath)")
        ;

    // declare the options allowed on command line and in config files
    po::options_description desc_generic ("Options allowed additionally in a config file");

        desc_generic.add_options()
            ("output,o", po::value<string>(), 
                "specify a file [arg] to use for output instead of stdout or "
                "disable output [-]")
            ("autooutput,E", 
                "output goes into a file named <input_basename>.i")
            ("include,I", po::value<cmd_line_utils::include_paths>()->composing(), 
                "specify an additional include directory")
            ("sysinclude,S", po::value<vector<string> >()->composing(), 
                "specify an additional system include directory")
            ("forceinclude,F", po::value<vector<string> >()->composing(),
                "force inclusion of the given file")
            ("define,D", po::value<vector<string> >()->composing(), 
                "specify a macro to define (as macro[=[value]])")
            ("predefine,P", po::value<vector<string> >()->composing(), 
                "specify a macro to predefine (as macro[=[value]])")
            ("undefine,U", po::value<vector<string> >()->composing(), 
                "specify a macro to undefine")
            ("nesting,n", po::value<int>(), 
                "specify a new maximal include nesting depth")
        ;
        
    po::options_description desc_ext ("Extended options (allowed everywhere)");

        desc_ext.add_options()
            ("traceto,t", po::value<string>(), 
                "output macro expansion tracing information to a file [arg] "
                "or to stderr [-]")
            ("timer", "output overall elapsed computing time to stderr")
            ("long_long", "enable long long support in C++ mode")
#if BOOST_WAVE_SUPPORT_VARIADICS_PLACEMARKERS != 0
            ("variadics", "enable certain C99 extensions in C++ mode")
            ("c99", "enable C99 mode (implies --variadics)")
#endif 
            ("listincludes,l", po::value<string>(), 
                "list names of included files to a file [arg] or to stdout [-]")
            ("preserve,p", po::value<int>()->default_value(0), 
                "preserve whitespace\n"
                            "0: no whitespace is preserved (default),\n"
                            "1: comments are preserved,\n" 
                            "2: all whitespace is preserved")
            ("line,L", po::value<int>()->default_value(1), 
                "control the generation of #line directives\n"
                            "0: no #line directives are generated\n"
                            "1: #line directives will be emitted (default)")
            ("extended,x", "enable the #pragma wave system() directive")
#if BOOST_WAVE_SUPPORT_PRAGMA_ONCE != 0
            ("noguard,G", "disable include guard detection")
#endif
#if BOOST_WAVE_SERIALIZATION != 0
            ("state,s", po::value<string>(), 
                "load and save state information from/to the given file [arg] "
                "or 'wave.state' [-] (interactive mode only)")
#endif
        ;
    
    // combine the options for the different usage schemes
    po::options_description desc_overall_cmdline;
    po::options_description desc_overall_cfgfile;

        desc_overall_cmdline.add(desc_cmdline).add(desc_generic).add(desc_ext);
        desc_overall_cfgfile.add(desc_generic).add(desc_ext);
        
    // parse command line and store results
        using namespace boost::program_options::command_line_style;

    po::parsed_options opts(po::parse_command_line(argc, argv, 
            desc_overall_cmdline, unix_style, cmd_line_utils::at_option_parser));
    po::variables_map vm;
    
        po::store(opts, vm);
        po::notify(vm);

    // Try to find a wave.cfg in the same directory as the executable was 
    // started from. If this exists, treat it as a wave config file
    fs::path filename(argv[0], fs::native);

        filename = filename.branch_path() / "wave.cfg";
        cmd_line_utils::read_config_file_options(filename.string(), 
            desc_overall_cfgfile, vm, true);

    // if there is specified at least one config file, parse it and add the 
    // options to the main variables_map
        if (vm.count("config-file")) {
            vector<string> const &cfg_files = 
                vm["config-file"].as<vector<string> >();
            vector<string>::const_iterator end = cfg_files.end();
            for (vector<string>::const_iterator cit = cfg_files.begin(); 
                 cit != end; ++cit)
            {
            // parse a single config file and store the results
                cmd_line_utils::read_config_file_options(*cit, 
                    desc_overall_cfgfile, vm);
            }
        }

    // ... act as required 
        if (vm.count("help")) {
        po::options_description desc_help (
            "Usage: wave [options] [@config-file(s)] [file]");

            desc_help.add(desc_cmdline).add(desc_generic).add(desc_ext);
            cout << desc_help << endl;
            return 1;
        }
        
        if (vm.count("version")) {
            cout << get_version() << endl;
            return 0;
        }

        if (vm.count("copyright")) {
            return print_copyright();
        }

    // extract the arguments from the parsed command line
    vector<po::option> arguments;
    
        std::remove_copy_if(opts.options.begin(), opts.options.end(), 
            back_inserter(arguments), cmd_line_utils::is_argument());
            
    // if there is no input file given, then take input from stdin
        if (0 == arguments.size() || 0 == arguments[0].value.size() ||
            arguments[0].value[0] == "-") 
        {
        // preprocess the given input from stdin
            return do_actual_work("<stdin>", std::cin, vm, true);
        }
        else {
        std::string file_name(arguments[0].value[0]);
        ifstream instream(file_name.c_str());

        // preprocess the given input file
            if (!instream.is_open()) {
                cerr << "wave: could not open input file: " << file_name << endl;
                return -1;
            }
            return do_actual_work(file_name, instream, vm, false);
        }
    }
    catch (std::exception const &e) {
        cout << "wave: exception caught: " << e.what() << endl;
        return 6;
    }
    catch (...) {
        cerr << "wave: unexpected exception caught." << endl;
        return 7;
    }
}

