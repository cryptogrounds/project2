USERNAME=zchampeau
CXX = g++
LD = g++
CXXFLAGS = -g -std=c++17
LDFLAGS = -g

TARGET = echo_s
OBJ_FILES = ${TARGET}.o
INC_FILES = ${TARGET}.h

${TARGET}: ${OBJ_FILES}
	${LD} ${LDFLAGS} ${OBJ_FILES} -o $@

%.o : %.cc ${INC_FILES}
	${CXX} -c ${CXXFLAGS} -o $@ $<

clean:
	rm -f core ${TARGET} ${OBJ_FILES}

submit:
	@if [ -z "${USERNAME}" ]; then \
		echo "USERNAME varaible is not set."; \
	else \
		echo "USERNAME variable is set to ${USERNAME}."; \
		rm -f core project1 ${OBJ_FILES}; \
		mkdir ${USERNAME}; \
		cp Makefile README.md *.h *.c ${USERNAME}; \
		tar zcf ${USERNAME}.tgz ${USERNAME}; \
		echo "Don't forget to upload ${USERNAME}.tgz to Canvas."; \
	fi
