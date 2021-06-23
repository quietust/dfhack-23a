OPTION(DL_RUBY "download libruby from the internet" ON)
SET(RUBYLIB ${CMAKE_CURRENT_BINARY_DIR}/libruby.dll)
download_file_unzip("https://github.com/DFHack/dfhack-bin/releases/download/0.44.09/win32-libruby187.dll.gz"
    "gz"
    ${RUBYLIB}.gz
    "ffc0f1b5b33748e2a36128e90c97f6b2"
    ${RUBYLIB}
    "482c1c418f4ee1a5f04203eee1cda0ef")

ADD_CUSTOM_COMMAND(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl ${dfhack_SOURCE_DIR}/library/include/df/codegen.out.xml ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb
    # cmake quirk: depending on codegen.out.xml or generate_headers only is not enough, needs both
    # test by manually patching any library/xml/moo.xml, run make ruby-autogen-rb -j2, and check build/plugins/ruby/ruby-autogen.rb for patched xml data
    DEPENDS generate_headers ${dfhack_SOURCE_DIR}/library/include/df/codegen.out.xml ${CMAKE_CURRENT_SOURCE_DIR}/codegen.pl
    COMMENT ruby-autogen.rb
)
ADD_CUSTOM_TARGET(ruby-autogen-rb DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb)

INCLUDE_DIRECTORIES("${dfhack_SOURCE_DIR}/depends/tthread")

DFHACK_PLUGIN(ruby ruby.cpp LINK_LIBRARIES dfhack-tinythread)
ADD_DEPENDENCIES(ruby ruby-autogen-rb)

INSTALL(FILES ${RUBYLIB} DESTINATION ${DFHACK_LIBRARY_DESTINATION})

INSTALL(DIRECTORY .
        DESTINATION hack/ruby
        FILES_MATCHING PATTERN "*.rb")

INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/ruby-autogen.rb DESTINATION hack/ruby)
