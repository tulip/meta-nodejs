DESCRIPTION = "Node.js is a JavaScript runtime built on Chrome's V8 JavaScript engine"
HOMEPAGE = "http://nodejs.org"
LICENSE = "MIT & BSD & Artistic-2.0"

COMPATIBLE_MACHINE_armv4 = "(!.*armv4).*"
COMPATIBLE_MACHINE_armv5 = "(!.*armv5).*"
COMPATIBLE_MACHINE_mips64 = "(!.*mips64).*"

INC_PR = "r1"
PR = "${INC_PR}.4"

DEPENDS = "openssl"
DEPENDS_append_class-target = " nodejs-native"

PROVIDES = "node"
RPROVIDES_${PN} = "node"

SRC_URI = "https://nodejs.org/dist/v${PV}/node-v${PV}.tar.gz;name=src \
           file://0003-Install-both-binaries-and-use-libdir.patch \
"

SRC_URI_append_class-target = " \
           file://0002-Using-native-binaries.patch \
"

S = "${WORKDIR}/node-v${PV}"

# v8 errors out if you have set CCACHE
CCACHE = ""

inherit nodejs-arch pkgconfig python3native

ARCHFLAGS_arm = "${@bb.utils.contains('TUNE_FEATURES', 'callconvention-hard', '--with-arm-float-abi=hard', '--with-arm-float-abi=softfp', d)} \
                 ${@bb.utils.contains('TUNE_FEATURES', 'neon', '--with-arm-fpu=neon', \
                    bb.utils.contains('TUNE_FEATURES', 'vfpv3d16', '--with-arm-fpu=vfpv3-d16', \
                    bb.utils.contains('TUNE_FEATURES', 'vfpv3', '--with-arm-fpu=vfpv3', \
                    '--with-arm-fpu=vfp', d), d), d)}"
ARCHFLAGS ?= ""

CROSSCONF = "--cross-compiling"
CROSSCONF_virtclass-native ="--no-cross-compiling"

GYP_DEFINES_append_mipsel = " mips_arch_variant='r1' "

PACKAGECONFIG ??= "ares icu zlib openssl"

PACKAGECONFIG[ares] = "--shared-cares,,c-ares"
PACKAGECONFIG[brotli] = "--shared-brotli,,brotli"
PACKAGECONFIG[icu] = "--with-intl=system-icu,--without-intl,icu"
PACKAGECONFIG[libuv] = "--shared-libuv,,libuv"
PACKAGECONFIG[nghttp2] = "--shared-nghttp2,,nghttp2"
PACKAGECONFIG[shared] = "--shared"
PACKAGECONFIG[openssl] = "--shared-openssl,,openssl"
PACKAGECONFIG[zlib] = "--shared-zlib,,zlib"

DISABLE_STATIC = ""

# We don't want to cross-compile during target compile,
# and we need to use the right flags during host compile,
# too.
EXTRA_OEMAKE = "\
    CC.host='${CC}' \
    CFLAGS.host='${CPPFLAGS} ${CFLAGS}' \
    CXX.host='${CXX}' \
    CXXFLAGS.host='${CPPFLAGS} ${CXXFLAGS}' \
    LDFLAGS.host='${LDFLAGS}' \
    AR.host='${AR}' \
    \
    builddir_name=./ \
"

do_configure () {
  export LD="${CXX}"
  GYP_DEFINES="${GYP_DEFINES}" export GYP_DEFINES
  python3 configure.py \
    --prefix="${prefix}" \
    --without-dtrace \
    --without-etw \
    --without-snapshot \
    --dest-cpu="${@nodejs_map_dest_cpu(d.getVar('TARGET_ARCH', True), d)}" \
    --dest-os=linux \
    --libdir=${D}${libdir} \
    ${CROSSCONF} \
    ${ARCHFLAGS} \
    ${PACKAGECONFIG_CONFARGS}
}

do_compile () {
  export LD="${CXX}"
  oe_runmake BUILDTYPE=Release
}

do_install () {
  oe_runmake install DESTDIR="${D}"

  # wasn't updated since 2009 and is the only thing requiring python2 in runtime
  # ERROR: nodejs-12.14.1-r0 do_package_qa: QA Issue: /usr/lib/node_modules/npm/node_modules/node-gyp/gyp/samples/samples
  #        contained in package nodejs-npm requires /usr/bin/python, but no providers found in RDEPENDS_nodejs-npm? [file-rdeps]
  rm -f ${D}${exec_prefix}/lib/node_modules/npm/node_modules/node-gyp/gyp/samples/samples
}

do_install_append_class-native() {
  # use node from PATH instead of absolute path to sysroot
  # node-v0.10.25/tools/install.py is using:
  # shebang = os.path.join(node_prefix, 'bin/node')
  # update_shebang(link_path, shebang)
  # and node_prefix can be very long path to bindir in native sysroot and
  # when it exceeds 128 character shebang limit it's stripped to incorrect path
  # and npm fails to execute like in this case with 133 characters show in log.do_install:
  # updating shebang of /home/jenkins/workspace/build-webos-nightly/device/qemux86/label/open-webos-builder/BUILD-qemux86/work/x86_64-linux/nodejs-native/0.10.15-r0/image/home/jenkins/workspace/build-webos-nightly/device/qemux86/label/open-webos-builder/BUILD-qemux86/sysroots/x86_64-linux/usr/bin/npm to /home/jenkins/workspace/build-webos-nightly/device/qemux86/label/open-webos-builder/BUILD-qemux86/sysroots/x86_64-linux/usr/bin/node
  # /usr/bin/npm is symlink to /usr/lib/node_modules/npm/bin/npm-cli.js
  # use sed on npm-cli.js because otherwise symlink is replaced with normal file and
  # npm-cli.js continues to use old shebang
  sed "1s^.*^#\!/usr/bin/env node^g" -i ${D}${exec_prefix}/lib/node_modules/npm/bin/npm-cli.js

  # Install the native binaries to provide it within sysroot for the target compilation
  install -d ${D}${bindir}
  install -m 0755 ${S}/out/Release/torque ${D}${bindir}/torque
  install -m 0755 ${S}/out/Release/bytecode_builtins_list_generator ${D}${bindir}/bytecode_builtins_list_generator
  if ${@bb.utils.contains('PACKAGECONFIG','icu','true','false',d)}; then
      install -m 0755 ${S}/out/Release/gen-regexp-special-case ${D}${bindir}/gen-regexp-special-case
  fi
  install -m 0755 ${S}/out/Release/mkcodecache ${D}${bindir}/mkcodecache
  install -m 0755 ${S}/out/Release/node_mksnapshot ${D}${bindir}/node_mksnapshot
}

do_install_append_class-target() {
  # make sure we use node from PATH instead of absolute path to sysroot
  sed "1s^.*^#\!${bindir}/env node^g" -i ${D}${exec_prefix}/lib/node_modules/npm/bin/npm-cli.js
}

PACKAGES =+ "${PN}-npm"
FILES_${PN}-npm = "${exec_prefix}/lib/node_modules ${bindir}/npm ${bindir}/npx"
#RDEPENDS_${PN}-npm = "bash python python-compiler python-shell python-datetime python-subprocess python-multiprocessing python-crypt python-textutils python-netclient python-misc"
RDEPENDS_${PN}-npm = "bash python3-core python3-shell python3-datetime \
    python3-misc python3-multiprocessing"

#PACKAGES =+ "${PN}-dtrace"
#FILES_${PN}-dtrace = "${exec_prefix}/lib/dtrace"

PACKAGES =+ "${PN}-systemtap"
FILES_${PN}-systemtap = "${datadir}/systemtap"

INSANE_SKIP_${PN} += "file-rdeps"
INSANE_SKIP_${PN}-dbg += "host-user-contaminated"

BBCLASSEXTEND = "native"
