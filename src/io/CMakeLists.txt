
set(io_SRC
	base64stream.cpp
	bufferstream.cpp
	gzipstream.cpp
	inkscapestream.cpp
	resource.cpp
	stringstream.cpp
	sys.cpp
	http.cpp
	uristream.cpp
	xsltstream.cpp

	# -------
	# Headers
	base64stream.h
	bufferstream.h
	gzipstream.h
	inkscapestream.h
	resource.h
	stringstream.h
	sys.h
	http.h
	uristream.h
	xsltstream.h
)

# add_inkscape_lib(io_LIB "${io_SRC}")
add_inkscape_source("${io_SRC}")
