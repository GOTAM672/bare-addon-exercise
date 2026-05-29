import { tcpCat } from './index.js'

async function main() {
  const ip = '1.1.1.1'
  const port = 80
  const payload = 'GET / HTTP/1.1\r\nHost: cloudflare.com\r\nConnection: close\r\n\r\n'

  console.log(`Connecting to TCP Socket -> ${ip}:${port}...\n`)

  try {
    const response = await tcpCat(ip, port, payload)
    console.log('Data successfully received:\n')
    console.log(response.toString('utf8'))

  } catch (error) {
    console.error('Connection failed with an error:', error.message)
  }
}

main()
