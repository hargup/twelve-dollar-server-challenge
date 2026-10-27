#!/usr/bin/env python3
"""Compare read-only HTTP requests with SQL on the original seed (not the live DB)."""
import json, os, random, socket, sqlite3, sys

seed = sys.argv[1]
count = int(sys.argv[2]) if len(sys.argv) > 2 else 20000
fields = ['id', 'body', 'created_at', 'author', 'like_count']
query = '''SELECT p.id, p.body, p.created_at, u.username,
                  (SELECT count(*) FROM likes l WHERE l.post_id=p.id)
           FROM posts p JOIN users u ON u.id=p.user_id WHERE p.id=?'''
db = sqlite3.connect('file:'+os.path.abspath(seed)+'?mode=ro', uri=True)
largest = db.execute('SELECT max(id) FROM posts').fetchone()[0]
rng = random.Random(42)
ids = [rng.randint(1, largest) for _ in range(count)]
expected = [dict(zip(fields, db.execute(query, (i,)).fetchone())) for i in ids]
db.close()
s = socket.create_connection((os.environ.get('HOST','127.0.0.1'), int(os.environ.get('PORT','3000'))), timeout=15)
pending = b''
try:
    for start in range(0, count, 32):
        batch = ids[start:start+32]
        s.sendall(b''.join(f'GET /posts/{i} HTTP/1.1\r\nHost: x\r\n\r\n'.encode() for i in batch))
        for i in range(len(batch)):
            while b'\r\n\r\n' not in pending:
                data = s.recv(65536)
                assert data, 'unexpected EOF'
                pending += data
            head, body = pending.split(b'\r\n\r\n',1)
            assert head.startswith(b'HTTP/1.1 200 '), head
            length = int(next(line.split(b':',1)[1] for line in head.split(b'\r\n') if line.lower().startswith(b'content-length:')))
            while len(body)<length:
                data = s.recv(65536)
                assert data, 'truncated response'
                body += data
            pending = body[length:]
            post = json.loads(body[:length])['post']
            assert list(post) == fields, post
            assert post == expected[start+i], (batch[i],post,expected[start+i])
finally:
    s.close()
print(f'SQL oracle: {count} random posts and exact key order passed')
