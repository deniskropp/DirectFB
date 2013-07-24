function(INSTALL_DIRECTFB_LIB target)
	set_target_properties(${target} PROPERTIES SOVERSION ${LIBVER})

	install(TARGETS ${target} LIBRARY DESTINATION lib )

	if(LINUX)
		set_target_properties(${target} PROPERTIES OUTPUT_NAME ${target}-${LIBVER} SOVERSION "0" VERSION "0.0.0")
		set(target_name lib${target}-${LIBVER}.so.0.0.0)

		add_custom_command(
			TARGET ${target}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E create_symlink ${target_name} ${CMAKE_CURRENT_BINARY_DIR}/lib${target}.so
			COMMAND ${CMAKE_COMMAND} -E create_symlink ${target_name} ${CMAKE_CURRENT_BINARY_DIR}/lib${target}-${LIBVER}.so.0
			COMMAND ${CMAKE_COMMAND} -E touch ${target_name}
		)

		install(
			FILES
			${CMAKE_CURRENT_BINARY_DIR}/lib${target}.so
			${CMAKE_CURRENT_BINARY_DIR}/lib${target}-${LIBVER}.so.0
			DESTINATION lib
		)

	endif(LINUX)
endfunction(INSTALL_DIRECTFB_LIB)

macro (DEFINE_DIRECTFB_MODULE target installname sources libs destination)
	add_library (${target} MODULE ${sources})
	target_link_libraries (${target} ${libs})
	set_target_properties (${target} PROPERTIES OUTPUT_NAME ${installname})
	install (TARGETS ${target} LIBRARY DESTINATION ${destination})
endmacro()


macro (DEFINE_DISPATCHER_MODULE interface)
	string (TOLOWER ${interface} INTERFACE_LOWERCASE)
	add_library (${INTERFACE_LOWERCASE}_dispatcher MODULE ${INTERFACE_LOWERCASE}_dispatcher.c)
	target_link_libraries (${INTERFACE_LOWERCASE}_dispatcher voodoo)
	install (TARGETS ${INTERFACE_LOWERCASE}_dispatcher LIBRARY DESTINATION ${INTERFACES_DIR}/${interface})
endmacro()

macro (DEFINE_REQUESTOR_MODULE interface)
	string (TOLOWER ${interface} INTERFACE_LOWERCASE)
	add_library (${INTERFACE_LOWERCASE}_requestor MODULE ${INTERFACE_LOWERCASE}_requestor.c)
	target_link_libraries (${INTERFACE_LOWERCASE}_requestor voodoo)
	install (TARGETS ${INTERFACE_LOWERCASE}_requestor LIBRARY DESTINATION ${INTERFACES_DIR}/${interface})
endmacro()

macro (DEFINE_DISPATCHER_MODULE_DEP interface deplibs)
	string (TOLOWER ${interface} INTERFACE_LOWERCASE)
	add_library (${INTERFACE_LOWERCASE}_dispatcher MODULE ${INTERFACE_LOWERCASE}_dispatcher.c)
	target_link_libraries (${INTERFACE_LOWERCASE}_dispatcher voodoo ${deplibs})
	install (TARGETS ${INTERFACE_LOWERCASE}_dispatcher LIBRARY DESTINATION ${INTERFACES_DIR}/${interface})
endmacro()

macro (DEFINE_REQUESTOR_MODULE_DEP interface deplibs)
	string (TOLOWER ${interface} INTERFACE_LOWERCASE)
	add_library (${INTERFACE_LOWERCASE}_requestor MODULE ${INTERFACE_LOWERCASE}_requestor.c)
	target_link_libraries (${INTERFACE_LOWERCASE}_requestor voodoo ${deplibs})
	install (TARGETS ${INTERFACE_LOWERCASE}_requestor LIBRARY DESTINATION ${INTERFACES_DIR}/${interface})
endmacro()

macro (FLUX_FILE inputdir inputfile)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${inputdir}/${inputfile}.cpp ${CMAKE_CURRENT_BINARY_DIR}/${inputdir}/${inputfile}.h
		COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/${inputdir}
		COMMAND ${FLUXCOMP} ${FLUXCOMP_OPTIONS} --object-ptrs --include-prefix=${inputdir} -o=${inputdir} ${CMAKE_CURRENT_SOURCE_DIR}/${inputdir}/${inputfile}.flux
		DEPENDS ${inputdir}/${inputfile}.flux
	)
endmacro()

macro (FLUX_FILE_SAWMAN inputfile)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${inputfile}.cpp ${CMAKE_CURRENT_BINARY_DIR}/${inputfile}.h
		COMMAND ${FLUXCOMP} ${FLUXCOMP_OPTIONS} --include-prefix=${inputdir} ${CMAKE_CURRENT_SOURCE_DIR}/${inputfile}.flux
		DEPENDS ${inputfile}.flux
	)
endmacro()



macro(CHECK_PTHREADS)
	if(LINUX)
		set(PTHREAD_CFLAGS "-D_REENTRANT")
		set(PTHREAD_LDFLAGS "-pthread")
	elseif(BSDI)
		set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
		set(PTHREAD_LDFLAGS "")
	elseif(DARWIN)
		set(PTHREAD_CFLAGS "-D_THREAD_SAFE")
		# causes Carbon.p complaints?
		# set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
		set(PTHREAD_LDFLAGS "")
	elseif(FREEBSD)
		set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
		set(PTHREAD_LDFLAGS "-pthread")
	elseif(NETBSD)
		set(PTHREAD_CFLAGS "-D_REENTRANT -D_THREAD_SAFE")
		set(PTHREAD_LDFLAGS "-lpthread")
	elseif(OPENBSD)
		set(PTHREAD_CFLAGS "-D_REENTRANT")
		set(PTHREAD_LDFLAGS "-pthread")
	elseif(SOLARIS)
		set(PTHREAD_CFLAGS "-D_REENTRANT")
		set(PTHREAD_LDFLAGS "-pthread -lposix4")
	elseif(SYSV5)
		set(PTHREAD_CFLAGS "-D_REENTRANT -Kthread")
		set(PTHREAD_LDFLAGS "")
	elseif(AIX)
		set(PTHREAD_CFLAGS "-D_REENTRANT -mthreads")
		set(PTHREAD_LDFLAGS "-pthread")
	elseif(HPUX)
		set(PTHREAD_CFLAGS "-D_REENTRANT")
		set(PTHREAD_LDFLAGS "-L/usr/lib -pthread")
	else()
		set(PTHREAD_CFLAGS "-D_REENTRANT")
		set(PTHREAD_LDFLAGS "-lpthread")
	endif(LINUX)
		# Run some tests
	set(CMAKE_REQUIRED_FLAGS "-D_GNU_SOURCE ${PTHREAD_CFLAGS} ${PTHREAD_LDFLAGS}")
	check_c_source_compiles("
			#include <pthread.h>
			int main(int argc, char** argv) {
				pthread_attr_t type;
				pthread_attr_init(&type);
				return 0;
			}" HAVE_PTHREADS)
	if(HAVE_PTHREADS)
		check_symbol_exists (PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP "pthread.h" HAVE_DECL_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
		check_c_source_compiles("
		#include <pthread.h>
		int main(int argc, char **argv) {
			pthread_mutexattr_t attr;
			pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
			return 0;
		}" HAVE_DECL_PTHREAD_MUTEX_RECURSIVE)
	endif(HAVE_PTHREADS)
endmacro(CHECK_PTHREADS)
