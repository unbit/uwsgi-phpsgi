import os
NAME='phpsgi'

PHPPATH = 'php-config'

CFLAGS = [os.popen(PHPPATH + ' --includes').read().rstrip(), '-Wno-sign-compare']
LDFLAGS = os.popen(PHPPATH + ' --ldflags').read().rstrip().split()
LIBS = [os.popen(PHPPATH + ' --libs').read().rstrip(), '-lphp5']

GCC_LIST = ['phpsgi_plugin']
