import sys
import re

def iter_code_chars(line):
    """Yield (index, ch) plus quote state; raise nothing."""
    in_s = False
    in_c = False
    esc = False
    i = 0
    while i < len(line):
        ch = line[i]
        if in_s or in_c:
            yield i, ch, True
            if esc:
                esc = False
            elif ch == '\\':
                esc = True
            elif in_s and ch == '"':
                in_s = False
            elif in_c and ch == "'":
                in_c = False
        else:
            if ch == '"':
                in_s = True
                yield i, ch, True
            elif ch == "'":
                in_c = True
                yield i, ch, True
            else:
                yield i, ch, False
        i += 1


def find_line_comment(line):
    """Return index of a // comment that starts outside strings/chars."""
    prev = ''
    for i, ch, quoted in iter_code_chars(line):
        if quoted:
            prev = ch
            continue
        if ch == '/' and prev == '/':
            return i - 1
        prev = ch
    return -1


def strip_trailing_comments(text):
    out = []
    removed = 0
    for line in text.split('\n'):
        idx = find_line_comment(line)
        if idx < 0:
            out.append(line)
            continue
        code = line[:idx]
        if code.strip() == '':
            # standalone comment: keep
            out.append(line.rstrip())
            continue
        removed += 1
        out.append(code.rstrip())
    return '\n'.join(out), removed


def split_statements(text):
    lines = []
    for line in text.split('\n'):
        stripped = line.lstrip()
        if stripped.startswith('#'):
            lines.append(line)
            continue
        if ';' not in line:
            lines.append(line)
            continue
        indent = line[:len(line) - len(stripped)]
        pieces = []
        buf = []
        depth = 0
        in_s = False
        in_c = False
        esc = False
        for ch in line:
            if in_s or in_c:
                buf.append(ch)
                if esc:
                    esc = False
                elif ch == '\\':
                    esc = True
                elif in_s and ch == '"':
                    in_s = False
                elif in_c and ch == "'":
                    in_c = False
                continue
            if ch == '"':
                in_s = True
                buf.append(ch)
                continue
            if ch == "'":
                in_c = True
                buf.append(ch)
                continue
            if ch in '([{':
                depth += 1
                buf.append(ch)
                continue
            if ch in ')]}':
                depth -= 1
                buf.append(ch)
                continue
            if ch == ';' and depth <= 0:
                pieces.append(''.join(buf) + ';')
                buf = []
                continue
            buf.append(ch)
        pieces.append(''.join(buf))
        if len(pieces) == 1:
            lines.append(line)
            continue
        for piece in pieces:
            body = piece.strip()
            if not body:
                continue
            lines.append((indent + body).rstrip())
    return '\n'.join(lines)


TOKEN_RE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*|\d+(?:\.\d+)?|//[^\n]*|/\*.*?\*/|\S', re.S)


def tokens(text):
    text = text.replace('\r\n', '\n')
    raw = TOKEN_RE.findall(text)
    out = []
    for tok in raw:
        if tok.startswith('//'):
            continue
        if tok.startswith('/*'):
            continue
        out.append(tok)
    return out


def main():
    mode = sys.argv[1]
    src = open(sys.argv[2], encoding='utf-8').read()
    if mode == 'strip':
        result, removed = strip_trailing_comments(src)
        open(sys.argv[3], 'w', encoding='utf-8', newline='\n').write(result)
        print('stripped trailing comments:', removed)
    elif mode == 'split':
        result = split_statements(src)
        open(sys.argv[3], 'w', encoding='utf-8', newline='\n').write(result)
        print('split done')
    elif mode == 'norm':
        open(sys.argv[3], 'w', encoding='utf-8', newline='\n').write(' '.join(tokens(src)))
        print('tokens:', len(tokens(src)))
    elif mode == 'diff':
        a = tokens(open(sys.argv[2], encoding='utf-8').read())
        b = tokens(open(sys.argv[3], encoding='utf-8').read())
        n = min(len(a), len(b))
        first = None
        for i in range(n):
            if a[i] != b[i]:
                first = i
                break
        if first is None and len(a) != len(b):
            first = n
        if first is None:
            print('IDENTICAL token streams:', len(a))
        else:
            print('DIFF at token', first, 'of', len(a), len(b))
            lo = max(0, first - 12)
            print('ORIG:', ' '.join(a[lo:first + 12]))
            print('NEW :', ' '.join(b[lo:first + 12]))


if __name__ == '__main__':
    main()
