#!/usr/bin/env python3
RED='\033[0;34m'
NC='\033[0m' # No Color

def test_run(target, out_file, in_file=None, add_returncode=False):
    in_str = b""
    if in_file != None:
        if not os.path.exists(in_file):
            print("Ignoring input file because not exist.")
            in_file = None
        else:
            with open(in_file, 'rb') as f:
                in_str = f.read()

    from subprocess import Popen, PIPE

    commands = [target]
    # print(' '.join(commands))

    p = Popen(' '.join(commands), stdout=PIPE,  stdin=PIPE, stderr=PIPE, shell=True)
    out, err = p.communicate(input=in_str)
    if add_returncode:
        code = p.returncode
        if len(out) > 0 and out[-1] != ord('\n'):
            out = out + b'\n'
        out = out + str(code).encode()

    with open(out_file, 'rb') as f:
        s = f.read()

    print('result: ',out)
    print('excepted: ',s)
    if out.replace(b'\r\n', b'\n').strip() == s.replace(b'\r\n', b'\n').strip():
        print(RED+"=========== Pass! ==============" +NC)
        return True
    s = s.rsplit(b"\n", 1)
    if len(s) > 1:
        s[0] = s[0].strip()
    s = b"\n".join(s)

    print("stderr: ", err, file=sys.stderr) # perfomance test打印所花时间
    if out.strip() == s.strip():
        print(RED+"=========== Pass! ==============" +NC)
        return True
    else:
        print(RED+"Result Mismatch"+NC)
        return False

if __name__ == '__main__':
    import sys, os, pathlib
    if len(sys.argv) < 3:
        print("Usage: python test_runner.py <target> <out_file> [in_file]")
        sys.exit(1)
    if len(sys.argv) == 3:
        test_run(sys.argv[1], sys.argv[2], add_returncode=True)
    elif len(sys.argv) == 4:
        test_run(sys.argv[1], sys.argv[2], sys.argv[3], add_returncode=True)
    elif len(sys.argv) == 5:
        ret = test_run(sys.argv[1], sys.argv[2], sys.argv[3], add_returncode=True)
        if ret:
            pathlib.Path(sys.argv[4]).touch()
        # else:
        #     exit(-1)
