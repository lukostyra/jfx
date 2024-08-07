macro(MAKE_HASH_TOOLS _source)
    get_filename_component(_name ${_source} NAME_WE)

    if (${_source} STREQUAL "DocTypeStrings")
        set(_hash_tools_h "${WebCore_DERIVED_SOURCES_DIR}/HashTools.h")
    else ()
        set(_hash_tools_h "")
    endif ()

    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/${_name}.cpp ${_hash_tools_h}
        MAIN_DEPENDENCY ${_source}.gperf
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/make-hash-tools.pl ${WebCore_DERIVED_SOURCES_DIR} ${_source}.gperf ${GPERF_EXECUTABLE}
        VERBATIM)

    unset(_name)
    unset(_hash_tools_h)
endmacro()


macro(MAKE_JS_FILE_ARRAYS _output_cpp _output_h _namespace _scripts _scripts_dependencies)
    add_custom_command(
        OUTPUT ${_output_h} ${_output_cpp}
        DEPENDS ${JavaScriptCore_SCRIPTS_DIR}/make-js-file-arrays.py ${${_scripts}}
        COMMAND ${PYTHON_EXECUTABLE} ${JavaScriptCore_SCRIPTS_DIR}/make-js-file-arrays.py --fail-if-non-ascii -n ${_namespace} ${_output_h} ${_output_cpp} ${${_scripts}}
        VERBATIM)
    WEBKIT_ADD_SOURCE_DEPENDENCIES(${${_scripts_dependencies}} ${_output_h} ${_output_cpp})
endmacro()


option(SHOW_BINDINGS_GENERATION_PROGRESS "Show progress of generating bindings" OFF)

# Helper macro which wraps generate-bindings-all.pl script.
#   target is a new target name to be added
#   OUTPUT_SOURCE is a list name which will contain generated sources.(eg. WebCore_SOURCES)
#   INPUT_FILES are IDL files to generate.
#   PP_INPUT_FILES are IDL files to preprocess.
#   BASE_DIR is base directory where script is called.
#   INCLUDED_FILES are additional IDL files that can be imported by the generator.
#   FEATURES is a value of --defines argument.
#   DESTINATION is a value of --outputDir argument.
#   GENERATOR is a value of --generator argument.
#   SUPPLEMENTAL_DEPFILE is a value of --supplementalDependencyFile. (optional)
#   PP_EXTRA_OUTPUT is extra outputs of preprocess-idls.pl. (optional)
#   PP_EXTRA_ARGS is extra arguments for preprocess-idls.pl. (optional)
function(GENERATE_BINDINGS target)
    set(options)
    set(oneValueArgs OUTPUT_SOURCE BASE_DIR FEATURES DESTINATION GENERATOR SUPPLEMENTAL_DEPFILE)
    set(multiValueArgs INPUT_FILES PP_INPUT_FILES INCLUDED_FILES PP_EXTRA_OUTPUT PP_EXTRA_ARGS)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(binding_generator ${WEBCORE_DIR}/bindings/scripts/generate-bindings-all.pl)
    set(idl_attributes_file ${WEBCORE_DIR}/bindings/scripts/IDLAttributes.json)
    set(idl_files_list ${CMAKE_CURRENT_BINARY_DIR}/idl_files_${target}.tmp)
    set(pp_idl_files_list ${CMAKE_CURRENT_BINARY_DIR}/pp_idl_files_${target}.tmp)
    set(included_idl_files_list ${CMAKE_CURRENT_BINARY_DIR}/included_idl_files_${target}.tmp)
    set(_supplemental_dependency)

    set(content)
    foreach (f ${arg_INPUT_FILES})
        if (NOT IS_ABSOLUTE ${f})
            set(f ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        endif ()
        set(content "${content}${f}\n")
    endforeach ()
    file(WRITE ${idl_files_list} ${content})

    set(pp_content)
    foreach (f ${arg_PP_INPUT_FILES})
        if (NOT IS_ABSOLUTE ${f})
            set(f ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        endif ()
        set(pp_content "${pp_content}${f}\n")
    endforeach ()
    file(WRITE ${pp_idl_files_list} ${pp_content})

    set(include_content)
    foreach (f ${arg_INPUT_FILES} ${arg_INCLUDED_FILES})
        if (NOT IS_ABSOLUTE ${f})
            set(f ${CMAKE_CURRENT_SOURCE_DIR}/${f})
        endif ()
        set(include_content "${include_content}${f}\n")
    endforeach ()
    file(WRITE ${included_idl_files_list} ${include_content})
    configure_file(${included_idl_files_list}  ${included_idl_files_list} @ONLY NEWLINE_STYLE LF)

    set(args
        --defines ${arg_FEATURES}
        --generator ${arg_GENERATOR}
        --outputDir ${arg_DESTINATION}
        --idlFilesList ${idl_files_list}
        --idlFileNamesList ${included_idl_files_list}
        --ppIDLFilesList ${pp_idl_files_list}
        --preprocessor "${CODE_GENERATOR_PREPROCESSOR}"
        --idlAttributesFile ${idl_attributes_file}
    )
    if (arg_SUPPLEMENTAL_DEPFILE)
        list(APPEND args --supplementalDependencyFile ${arg_SUPPLEMENTAL_DEPFILE})
    endif ()
    ProcessorCount(PROCESSOR_COUNT)
    if (PROCESSOR_COUNT)
        list(APPEND args --numOfJobs ${PROCESSOR_COUNT})
    endif ()

    # https://support.microsoft.com/en-in/help/830473/command-prompt-cmd-exe-command-line-string-limitation
    # pass --include dir list to tmp file instead of multiple argument
    if (WIN32 AND PORT STREQUAL "Java")
        set(include_dir_list ${CMAKE_CURRENT_BINARY_DIR}/include_dir_${target}.tmp)
        set(includeDirectories)
        foreach (i IN LISTS arg_IDL_INCLUDES)
            if (IS_ABSOLUTE ${i})
                set(includeDirectories "${includeDirectories}${i}\n")
            else ()
                set(includeDirectories "${includeDirectories}${CMAKE_CURRENT_SOURCE_DIR}/${i}\n")
            endif ()
        endforeach ()
        file(WRITE ${include_dir_list} ${includeDirectories})
        list(APPEND args --includeDirlist ${include_dir_list})
    else ()
            foreach (i IN LISTS arg_IDL_INCLUDES)
            if (IS_ABSOLUTE ${i})
                list(APPEND args --include ${i})
            else ()
                list(APPEND args --include ${CMAKE_CURRENT_SOURCE_DIR}/${i})
            endif ()
        endforeach ()
    endif ()

    foreach (i IN LISTS arg_PP_EXTRA_OUTPUT)
        list(APPEND args --ppExtraOutput ${i})
    endforeach ()
    foreach (i IN LISTS arg_PP_EXTRA_ARGS)
        list(APPEND args --ppExtraArgs ${i})
    endforeach ()

    set(common_generator_dependencies
        ${WEBCORE_DIR}/bindings/scripts/generate-bindings.pl
        ${SCRIPTS_BINDINGS}
        # Changing enabled features should trigger recompiling all IDL files
        # because some of them use #if.
        ${CMAKE_BINARY_DIR}/cmakeconfig.h
        # Settings can be removed also which requires regeneration.
        ${WTF_WEB_PREFERENCES}
    )
    if (EXISTS ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${arg_GENERATOR}.pm)
        list(APPEND common_generator_dependencies ${WEBCORE_DIR}/bindings/scripts/CodeGenerator${arg_GENERATOR}.pm)
    endif ()
    if (EXISTS ${arg_BASE_DIR}/CodeGenerator${arg_GENERATOR}.pm)
        list(APPEND common_generator_dependencies ${arg_BASE_DIR}/CodeGenerator${arg_GENERATOR}.pm)
    endif ()
    foreach (i IN LISTS common_generator_dependencies)
        list(APPEND args --generatorDependency ${i})
    endforeach ()

    set(gen_sources)
    set(gen_headers)
    foreach (_file ${arg_INPUT_FILES})
        get_filename_component(_name ${_file} NAME_WE)
        list(APPEND gen_sources ${arg_DESTINATION}/JS${_name}.cpp)
        list(APPEND gen_headers ${arg_DESTINATION}/JS${_name}.h)
    endforeach ()
    set(${arg_OUTPUT_SOURCE} ${${arg_OUTPUT_SOURCE}} ${gen_sources} PARENT_SCOPE)
    set(act_args)
    if (SHOW_BINDINGS_GENERATION_PROGRESS)
        list(APPEND args --showProgress)
    endif ()
    list(APPEND act_args BYPRODUCTS ${gen_sources} ${gen_headers})
    if (SHOW_BINDINGS_GENERATION_PROGRESS)
        list(APPEND act_args USES_TERMINAL)
    endif ()
    add_custom_target(${target}
        COMMAND ${PERL_EXECUTABLE} ${binding_generator} ${args}
        DEPENDS ${arg_INPUT_FILES} ${arg_PP_INPUT_FILES}
        WORKING_DIRECTORY ${arg_BASE_DIR}
        COMMENT "Generate bindings (${target})"
        VERBATIM ${act_args})
endfunction()


macro(GENERATE_FONT_NAMES _infile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --fonts ${_infile})
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/WebKitFontFamilyNames.cpp ${WebCore_DERIVED_SOURCES_DIR}/WebKitFontFamilyNames.h)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --outputDir ${WebCore_DERIVED_SOURCES_DIR} ${_arguments}
        VERBATIM)
endmacro()


macro(GENERATE_EVENT_FACTORY _infile _namespace)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_event_factory.pl)
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Interfaces.h ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Factory.cpp)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --input ${_infile} --outputDir ${WebCore_DERIVED_SOURCES_DIR}
        VERBATIM)
endmacro()


macro(GENERATE_EVENT_NAMES _infile)
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make-event-names.py)
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/EventNames.h ${WebCore_DERIVED_SOURCES_DIR}/EventNames.cpp)

    add_custom_command(
        OUTPUT  ${_outputfiles}
        MAIN_DEPENDENCY ${_infile}
        DEPENDS ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS}
        WORKING_DIRECTORY ${WebCore_DERIVED_SOURCES_DIR}
        COMMAND ${PYTHON_EXECUTABLE} ${NAMES_GENERATOR} --event-names ${_infile}
        VERBATIM)
endmacro()


function(GENERATE_DOM_NAMES _namespace _attrs)
    if (ARGN)
        list(GET ARGN 0 _elements)
        list(REMOVE_AT ARGN 0)
    endif ()
    set(NAMES_GENERATOR ${WEBCORE_DIR}/dom/make_names.pl)
    set(_arguments  --attrs ${_attrs})
    set(_outputfiles ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Names.cpp ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}Names.h)

    if (_elements)
        set(_arguments "${_arguments}" --elements ${_elements} --factory --wrapperFactory)
        set(_outputfiles "${_outputfiles}" ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementFactory.h ${WebCore_DERIVED_SOURCES_DIR}/${_namespace}ElementTypeHelpers.h ${WebCore_DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.cpp ${WebCore_DERIVED_SOURCES_DIR}/JS${_namespace}ElementWrapperFactory.h)
    endif ()

    add_custom_command(
        OUTPUT  ${_outputfiles}
        DEPENDS ${MAKE_NAMES_DEPENDENCIES} ${NAMES_GENERATOR} ${SCRIPTS_BINDINGS} ${_attrs} ${_elements}
        COMMAND ${PERL_EXECUTABLE} ${NAMES_GENERATOR} --outputDir ${WebCore_DERIVED_SOURCES_DIR} ${_arguments} ${_additionArguments}
        VERBATIM)
endfunction()


function(GENERATE_DOM_NAME_ENUM _enum)
    add_custom_command(
        OUTPUT ${WebCore_DERIVED_SOURCES_DIR}/${_enum}.cpp ${WebCore_DERIVED_SOURCES_DIR}/${_enum}.h
        DEPENDS ${WEBCORE_DIR}/html/HTMLTagNames.in ${WEBCORE_DIR}/svg/svgtags.in ${WEBCORE_DIR}/mathml/mathtags.in ${WEBCORE_DIR}/html/HTMLAttributeNames.in ${WEBCORE_DIR}/mathml/mathattrs.in ${WEBCORE_DIR}/svg/svgattrs.in ${WEBCORE_DIR}/svg/xlinkattrs.in ${WEBCORE_DIR}/xml/xmlattrs.in ${WEBCORE_DIR}/xml/xmlnsattrs.in ${MAKE_NAMES_DEPENDENCIES} ${WEBCORE_DIR}/dom/make_names.pl  ${SCRIPTS_BINDINGS}
        COMMAND ${PERL_EXECUTABLE} ${WEBCORE_DIR}/dom/make_names.pl --outputDir ${WebCore_DERIVED_SOURCES_DIR} --enum ${_enum} --elements ${WEBCORE_DIR}/html/HTMLTagNames.in --elements ${WEBCORE_DIR}/svg/svgtags.in --elements ${WEBCORE_DIR}/mathml/mathtags.in --attrs ${WEBCORE_DIR}/html/HTMLAttributeNames.in --attrs ${WEBCORE_DIR}/mathml/mathattrs.in --attrs ${WEBCORE_DIR}/svg/svgattrs.in --attrs ${WEBCORE_DIR}/svg/xlinkattrs.in --attrs ${WEBCORE_DIR}/xml/xmlattrs.in --attrs ${WEBCORE_DIR}/xml/xmlnsattrs.in
        VERBATIM)
endfunction()
