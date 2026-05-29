const resolved = Bare.Addon.resolve('.', import.meta.url)
const addon = Bare.Addon.load(resolved).exports

export async function tcpCat(ip, port, payload) {
  if (typeof ip !== 'string') throw new TypeError('IP must be a string')
  if (typeof port !== 'number') throw new TypeError('Port must be a number')

  const bufferPayload = Buffer.isBuffer(payload) ? payload : Buffer.from(payload)

  const arrayBuffer = await addon.tcpCat(ip, port, bufferPayload)

  return Buffer.from(arrayBuffer)
}
