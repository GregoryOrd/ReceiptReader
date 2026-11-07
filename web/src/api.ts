import createClient from 'openapi-fetch'
import type { paths } from './openapi'

export const api = createClient<paths>({ baseUrl: '/api/v1' })
