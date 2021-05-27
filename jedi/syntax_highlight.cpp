#include "syntax_highlight.h"
#include "utils.h"

#include <cassert>
#include <fstream>
#include <json.hpp>

#include <jtk/file_utils.h>

namespace
  {

  std::vector<std::wstring> break_string(std::string in)
    {
    std::vector<std::wstring> out;
    while (!in.empty())
      {
      auto splitpos = in.find_first_of(' ');
      std::string keyword = in.substr(0, splitpos);
      if (splitpos != std::string::npos)
        in.erase(0, splitpos + 1);
      else
        in.clear();
      out.push_back(jtk::convert_string_to_wstring(keyword));
      }
    return out;
    }

  keyword_data make_keyword_data_for_cpp()
    {
    keyword_data kd;

    std::string in = "alignof and and_eq bitandbitor break case catch compl const_cast continue default delete do dynamic_cast else false for goto if namespace new not not_eq nullptr operator or or_eq reinterpret_cast return sizeof static_assert static_cast switch this throw true try typedef typeid using while xor xor_eq NULL";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "alignas asm auto bool char char16_t char32_t class clock_t const constexpr decltype double enum explicit export extern final float friend inline int int8_t int16_t int32_t int64_t int_fast8_t int_fast16_t int_fast32_t int_fast64_t intmax_t intptr_t long mutable noexcept override private protected ptrdiff_t public register short signed size_t ssize_t static struct template thread_local time_t typename uint8_t uint16_t uint32_t uint64_t uint_fast8_t uint_fast16_t uint_fast32_t uint_fast64_t uintmax_t uintptr_t union unsigned virtual void volatile wchar_t";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }

  keyword_data make_keyword_data_for_c()
    {
    keyword_data kd;

    std::string in = "if else switch case default break goto return for while do continue typedef sizeof NULL";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "void struct union enum char short int long double float signed unsigned const static extern auto register volatile bool uint8_t uint16_t uint32_t uint64_t int8_t int16_t int32_t int64_t size_t time_t clock_t wchar_t FILE";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }

  keyword_data make_keyword_data_for_scm()
    {
    keyword_data kd;

    std::string in = "+ - * / = < > <= >= => abs acos and angle append apply asin assoc assq assv atan begin boolean? caar cadr call-with-current-continuation call/cc call-with-input-file call-with-output-file call-with-values car cdr cdar cddr caaar caadr cadar caddr cdaar cdadr cddar cdddr caaaar caaadr caadar caaddr cadaar cadadr caddar cadddr cdaaar cdaadr cdadar cdaddr cddaar cddadr cdddar cddddr case ceiling char->integer char-alphabetic? char-ci<=? char-ci<? char-ci=? char-ci>=? char-ci>? char-downcase char-lower-case? char-numeric? char-ready? char-upcase char-upper-case? char-whitespace? char<=? char<? char=? char>=? char>? char? close-input-port close-output-port complex? cond cons cos current-input-port current-output-port define define-syntax delay denominator display do dynamic-wind else eof-object? eq? equal? eqv? eval even? exact->inexact exact? exp expt floor for-each force gcd if imag-part inexact->exact inexact? input-port? integer-char integer? interaction-environment lambda lcm length let let* let-syntax letrec letrec-syntax list list->string list->vector list-ref list-tail list? load log magnitude make-polar make-rectangular make-string make-vector map max member memq memv min modulo negative? newline not null-environment null? number->string number? numerator odd? open-input-file open-output-file or output-port? pair? peek-char positive? procedure? quasiquote quote quotient rational? rationalize read read-char real-part real? remainder reverse round scheme-report-environment set! set-car! set-cdr! sin sqrt string string->list string->number string->symbol string-append string-ci<=? string-ci<? string-ci=? string-ci>=? string-ci>? string-copy string-fill! string-length string-ref string-set! string<=? string<? string=? string>=? string>? string? substring symbol->string symbol? syntax-rules transcript-off transcript-on truncate unquote unquote-splicing values vector vector->list vector-fill! vector-length vector-ref vector-set! vector? with-input-from-file with-output-to-file write write-char zero?";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    return kd;
    }

  keyword_data make_keyword_data_for_cmake()
    {
    keyword_data kd;

    std::string in = "add_custom_command add_custom_target add_definitions add_dependencies add_executable add_library add_subdirectory add_test aux_source_directory build_command build_name cmake_minimum_required configure_file create_test_sourcelist else elseif enable_language enable_testing endforeach endif endmacro endwhile exec_program execute_process export_library_dependencies file find_file find_library find_package find_path find_program fltk_wrap_ui foreach get_cmake_property get_directory_property get_filename_component get_source_file_property get_target_property get_test_property if include include_directories include_external_msproject include_regular_expression install install_files install_programs install_targets link_directories link_libraries list load_cache load_command macro make_directory mark_as_advanced math message option output_required_files project qt_wrap_cpp qt_wrap_ui remove remove_definitions separate_arguments set set_directory_properties set_source_files_properties set_target_properties set_tests_properties site_name source_group string subdir_depends subdirs target_link_libraries try_compile try_run use_mangled_mesa utility_source variable_requires vtk_make_instantiator vtk_wrap_java vtk_wrap_python vtk_wrap_tcl while write_file";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "ABSOLUTE ABSTRACT ADDITIONAL_MAKE_CLEAN_FILES ALL AND APPEND ARGS ASCII BEFORE CACHE CACHE_VARIABLES CLEAR COMMAND COMMANDS COMMAND_NAME COMMENT COMPARE COMPILE_FLAGS COPYONLY DEFINED DEFINE_SYMBOL DEPENDS DOC EQUAL ESCAPE_QUOTES EXCLUDE EXCLUDE_FROM_ALL EXISTS EXPORT_MACRO EXT EXTRA_INCLUDE FATAL_ERROR FILE FILES FORCE FUNCTION GENERATED GLOB GLOB_RECURSE GREATER GROUP_SIZE HEADER_FILE_ONLY HEADER_LOCATION IMMEDIATE INCLUDES INCLUDE_DIRECTORIES INCLUDE_INTERNALS INCLUDE_REGULAR_EXPRESSION LESS LINK_DIRECTORIES LINK_FLAGS LOCATION MACOSX_BUNDLE MACROS MAIN_DEPENDENCY MAKE_DIRECTORY MATCH MATCHALL MATCHES MODULE NAME NAME_WE NOT NOTEQUAL NO_SYSTEM_PATH OBJECT_DEPENDS OPTIONAL OR OUTPUT OUTPUT_VARIABLE PATH PATHS POST_BUILD POST_INSTALL_SCRIPT PREFIX PREORDER PRE_BUILD PRE_INSTALL_SCRIPT PRE_LINK PROGRAM PROGRAM_ARGS PROPERTIES QUIET RANGE READ REGEX REGULAR_EXPRESSION REPLACE REQUIRED RETURN_VALUE RUNTIME_DIRECTORY SEND_ERROR SHARED SOURCES STATIC STATUS STREQUAL STRGREATER STRLESS SUFFIX TARGET TOLOWER TOUPPER VAR VARIABLES VERSION WIN32 WRAP_EXCLUDE WRITE APPLE MINGW MSYS CYGWIN BORLAND WATCOM MSVC MSVC_IDE MSVC60 MSVC70 MSVC71 MSVC80 CMAKE_COMPILER_2005 OFF ON";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }

  keyword_data make_keyword_data_for_python()
    {
    keyword_data kd;

    std::string in = "and as assert break class continue def del elif else except exec False finally for from global if import in is lambda None not or pass print raise return True try while with yield async await";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    return kd;
    }

  keyword_data make_keyword_data_for_swift()
    {
    keyword_data kd;

    std::string in = "class deinit enum extension func import init internal let operator private protocol public static struct subscript typealias var keywordclass.swift.statements=break case continue default do else fallthrough for if in return switch where while";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "as dynamicType false is nil self Self super true __COLUMN__ __FILE__ __FUNCTION__ __LINE__ associativity convenience dynamic didSet final get infix inout lazy left mutating none nonmutating optional override postfix precedence prefix Protocol required right set Type unowned weak willSet";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }

  keyword_data make_keyword_data_for_objective_c()
    {
    keyword_data kd;

    std::string in = "if else switch case default break goto return for while do continue typedef sizeof NULL self super nil NIL interface implementation protocol end private protected public class selector encode defs";
    kd.keywords_1 = break_string(in);
    std::sort(kd.keywords_1.begin(), kd.keywords_1.end());

    in = "void struct union enum char short int long double float signed unsigned const static extern auto register volatile id Class SEL IMP BOOL oneway in out inout bycopy byref";
    kd.keywords_2 = break_string(in);
    std::sort(kd.keywords_2.begin(), kd.keywords_2.end());
    return kd;
    }

  comment_data make_comment_data_for_cpp()
    {
    comment_data cd;
    cd.multiline_begin = "/*";
    cd.multiline_end = "*/";
    cd.multistring_begin = "R\"(";
    cd.multistring_end = ")\"";
    cd.single_line = "//";
    cd.uses_quotes_for_chars = true;
    return cd;
    }

  comment_data make_comment_data_for_swift()
    {
    comment_data cd;
    cd.multiline_begin = "/*";
    cd.multiline_end = "*/";
    cd.multistring_begin = "\"\"\"";
    cd.multistring_end = "\"\"\"";
    cd.single_line = "//";
    cd.uses_quotes_for_chars = true;
    return cd;
    }

  comment_data make_comment_data_for_objective_c()
    {
    comment_data cd;
    cd.multiline_begin = "/*";
    cd.multiline_end = "*/";
    cd.single_line = "//";
    cd.uses_quotes_for_chars = true;
    return cd;
    }

  comment_data make_comment_data_for_assembly()
    {
    comment_data cd;
    cd.single_line = ";";
    return cd;
    }

  comment_data make_comment_data_for_scheme()
    {
    comment_data cd;
    cd.multiline_begin = "#|";
    cd.multiline_end = "|#";
    cd.single_line = ";";
    return cd;
    }

  comment_data make_comment_data_for_python_and_cmake()
    {
    comment_data cd;
    cd.single_line = "#";
    cd.uses_quotes_for_chars = true;
    return cd;
    }

  comment_data make_comment_data_for_xml()
    {
    comment_data cd;
    cd.multiline_begin = "<!--";
    cd.multiline_end = "-->";
    return cd;
    }

  comment_data make_comment_data_for_forth()
    {
    comment_data cd;
    cd.multiline_begin = "(";
    cd.multiline_end = ")";
    cd.single_line = "\\\\";
    return cd;
    }

  std::map<std::string, comment_data> build_comment_data_hardcoded()
    {
    std::map<std::string, comment_data> m;
    /*
    m["c"] = make_comment_data_for_cpp();
    m["cc"] = make_comment_data_for_cpp();
    m["cpp"] = make_comment_data_for_cpp();
    m["h"] = make_comment_data_for_cpp();
    m["hpp"] = make_comment_data_for_cpp();

    m["scm"] = make_comment_data_for_scheme();

    m["py"] = make_comment_data_for_python_and_cmake();
    m["cmake"] = make_comment_data_for_python_and_cmake();

    m["cmakelists.txt"] = make_comment_data_for_python_and_cmake();

    m["xml"] = make_comment_data_for_xml();
    m["html"] = make_comment_data_for_xml();

    m["s"] = make_comment_data_for_assembly();
    m["asm"] = make_comment_data_for_assembly();

    m["4th"] = make_comment_data_for_forth();

    m["swift"] = make_comment_data_for_swift();

    m["m"] = make_comment_data_for_objective_c();
    m["mm"] = make_comment_data_for_objective_c();
    */
    return m;
    }

  std::map<std::string, keyword_data> build_keyword_data_hardcoded()
    {
    std::map<std::string, keyword_data> m;
    /*
    m["cc"] = make_keyword_data_for_cpp();
    m["cpp"] = make_keyword_data_for_cpp();
    m["h"] = make_keyword_data_for_cpp();
    m["hpp"] = make_keyword_data_for_cpp();

    m["c"] = make_keyword_data_for_c();

    m["scm"] = make_keyword_data_for_scm();

    m["py"] = make_keyword_data_for_python();

    m["cmake"] = make_keyword_data_for_cmake();
    m["cmakelists.txt"] = make_keyword_data_for_cmake();

    m["swift"] = make_keyword_data_for_swift();

    m["m"] = make_keyword_data_for_objective_c();
    m["mm"] = make_keyword_data_for_objective_c();
    */
    return m;
    }


  void read_syntax_from_json(std::map<std::string, comment_data>& m, std::map<std::string, keyword_data>& k, const std::string& filename)
    {
    nlohmann::json j;
    
    std::ifstream i(filename);
    if (i.is_open())
      {
      try
        {
        i >> j;

        for (auto ext_it = j.begin(); ext_it != j.end(); ++ext_it)
          {
          auto element = *ext_it;
          if (element.is_object())
            {
            comment_data cd;
            keyword_data kd;
            for (auto it = element.begin(); it != element.end(); ++it)
              {
              if (it.key() == "multiline_comment_begin")
                {
                if (it.value().is_string())
                  cd.multiline_begin = it.value().get<std::string>();
                }
              if (it.key() == "multiline_comment_end")
                {
                if (it.value().is_string())
                  cd.multiline_end = it.value().get<std::string>();
                }
              if (it.key() == "singleline_comment")
                {
                if (it.value().is_string())
                  cd.single_line = it.value().get<std::string>();
                }
              if (it.key() == "multiline_string_begin")
                {
                if (it.value().is_string())
                  cd.multistring_begin = it.value().get<std::string>();
                }
              if (it.key() == "multiline_string_end")
                {
                if (it.value().is_string())
                  cd.multistring_end = it.value().get<std::string>();
                }
              if (it.key() == "uses_quotes_for_chars")
                {
                if (it.value().is_number_integer())
                  cd.uses_quotes_for_chars = it.value().get<int>() != 0;
                }
              if (it.key() == "keywords_1")
                {
                if (it.value().is_string())
                  kd.keywords_1 = break_string(it.value().get<std::string>());
                }
              if (it.key() == "keywords_2")
                {
                if (it.value().is_string())
                  kd.keywords_2 = break_string(it.value().get<std::string>());
                }
              }
            auto extensions = break_string(ext_it.key());
            for (const auto& we : extensions)
              {
              std::string e = jtk::convert_wstring_to_string(we);
              m[e] = cd;
              k[e] = kd;
              }
            }
          }
        }
      catch (nlohmann::detail::exception e)
        {
        }
      i.close();
      }
    }
  }

syntax_highlighter::syntax_highlighter()
  {
  extension_to_data = build_comment_data_hardcoded();
  extension_to_keywords = build_keyword_data_hardcoded();
  read_syntax_from_json(extension_to_data, extension_to_keywords, get_file_in_executable_path("jed_syntax.json"));
  for (auto& kd : extension_to_keywords)
    {
    std::sort(kd.second.keywords_1.begin(), kd.second.keywords_1.end());
    std::sort(kd.second.keywords_2.begin(), kd.second.keywords_2.end());
    }
  }

syntax_highlighter::~syntax_highlighter()
  {
  }

bool syntax_highlighter::extension_or_filename_has_syntax_highlighter(const std::string& ext_or_filename) const
  {
  return extension_to_data.find(ext_or_filename) != extension_to_data.end();
  }

bool syntax_highlighter::extension_or_filename_has_keywords(const std::string& ext_or_filename) const
  {
  return extension_to_keywords.find(ext_or_filename) != extension_to_keywords.end();
  }

comment_data syntax_highlighter::get_syntax_highlighter(const std::string& ext_or_filename) const
  {
  assert(extension_or_filename_has_syntax_highlighter(ext_or_filename));
  return extension_to_data.find(ext_or_filename)->second;
  }

const keyword_data& syntax_highlighter::get_keywords(const std::string& ext_or_filename) const
  {
  assert(extension_or_filename_has_keywords(ext_or_filename));
  return extension_to_keywords.find(ext_or_filename)->second;
  }
