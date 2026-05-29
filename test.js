import test from 'brittle'
import { tcpCat } from './index.js'

test('fetch response from Cloudflare', async (t) => {
  t.plan(2)
  try {
    const payload = 'GET / HTTP/1.1\r\nHost: cloudflare.com\r\nConnection: close\r\n\r\n'
    const response = await tcpCat('1.1.1.1', 80, payload)

    t.is(response instanceof Uint8Array, true, 'returns Uint8Array')
    
    const text = Buffer.from(response).toString('utf8')
    t.is(text.includes('HTTP/1.1'), true, 'contains HTTP response')
  } catch (err) {
    t.fail(err.message)
  }
})

test('reject on connection failure', async (t) => {
  t.plan(1)
  try {
    await tcpCat('127.0.0.1', 9999, 'ping')
    t.fail('expected connection error')
  } catch (err) {
    t.pass(`connection error received`)
  }
})
