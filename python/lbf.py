#!/usr/bin/env python
import getopt
import os
import os.path
import sys
import lbf

def _usage():
    print sys.argv[0], "--file lbffile [--ls] [--copy] [--clear-animations]"

def _main():
    lbffile = ''
    mode = 'list'
    outfile = ''

    try:
        opts,args = getopt.getopt(sys.argv[1:], "f:o:", ["file=","out=","ls","copy","clear-animations"])
        for opt,val in opts:
            if opt in ('-f','--file'):
                lbffile = val
            elif opt in ('-o', '--out'):
                outfile = val
            elif opt == '--ls':
                mode = 'list'
            elif opt == '--copy':
                mode = 'copy'
            elif opt == '--clear-animations':
                mode = 'clear_anim'

    except getopt.GetoptError:
        _usage()
        sys.exit(1)

    if not os.path.exists(lbffile):
        print lbffile, "does not exist"
        sys.exit(2)

    if mode == 'list':
        try:
            print "Loading",lbffile
            lbftop = lbf.parseLBF(lbffile)
            print "LBF File Version:",str(lbftop.major_version) + "." + str(lbftop.minor_version)
            lbf.list_neighbors(lbftop.first_node)
        except lbf.LBFError as err:
            print "Error occurred:\n",err

    elif mode == 'copy':
        if lbffile == outfile:
            print "Cannot copy to same file"
            sys.exit(3)

        try:
            print "Copying %s to %s" % (lbffile,outfile)
            inlbf = lbf.parseLBF(lbffile)
            lbf.writeLBF(inlbf, outfile)
        except lbf.LBFError as err:
            print "Error occurred\n",err

    elif mode == 'clear_anim':
        if len(outfile) == 0:
            outfile = lbffile

        try:
            print "Loading", lbffile
            lbftop = lbf.parseLBF(lbffile)
            print "LBF File Version:",str(lbftop.major_version) + "." + str(lbftop.minor_version)

            animSection = lbftop.find('ANIM_SECTION')
            if animSection:
                animNode = animSection.find('ANIMATION')
                if animNode:
                    animCount = 0
                    while animNode:
                        animCount += 1
                        animSection.remove(animNode)
                        animNode = animSection.find('ANIMATION')
                    print "Clearing %s animations..." % (animCount)
                    print "Writing to %s " % (outfile)
                    lbf.writeLBF(lbftop, outfile)
                else:
                    print "No animations found."
            else:
                print "No animation section found."
        except lbf.LBFError as err:
            print "Error occurred\n", err
    return

if __name__ == "__main__":
    _main()
