<script setup lang="ts">
import { computed, nextTick, onBeforeUnmount, onMounted, ref } from 'vue'
import Chart from 'chart.js/auto'
import type { components } from './openapi'
import { api } from './api'

type Summary = components['schemas']['PriceHistorySummary']
type ReceiptItem = components['schemas']['ReceiptItem']
type BatchJob = components['schemas']['BatchJob']

type Filters = {
  code: string
  desc: string
  price_min: number | undefined
  price_max: number | undefined
  date_start: string
  date_end: string
}

const filters = ref<Filters>({
  code: '',
  desc: '',
  price_min: undefined,
  price_max: undefined,
  date_start: '',
  date_end: '',
})
const summaries = ref<Summary[]>([])
const history = ref<ReceiptItem[]>([])
const selectedSummary = ref<Summary | null>(null)
const loadingSearch = ref(false)
const loadingHistory = ref(false)
const serverOnline = ref(false)
const notice = ref('')
const errorMessage = ref('')
const imageFile = ref<File | null>(null)
const imageItems = ref<ReceiptItem[]>([])
const receiptDate = ref(new Date().toISOString().slice(0, 10))
const processingImage = ref(false)
const confirmingItems = ref(false)
const batchFiles = ref<File[]>([])
const batchJob = ref<BatchJob | null>(null)
const processingBatch = ref(false)
const chartCanvas = ref<HTMLCanvasElement | null>(null)
let priceChart: Chart | undefined
let pollingTimer: number | undefined

const canConfirm = computed(() => imageItems.value.length > 0 && !confirmingItems.value)

function clearMessages() {
  notice.value = ''
  errorMessage.value = ''
}

function reportError(error: unknown) {
  errorMessage.value = error instanceof Error ? error.message : 'The request failed.'
}

async function checkServer() {
  try {
    const response = await api.GET('/health')
    serverOnline.value = response.response.ok && response.data?.status === 'ok'
  } catch {
    serverOnline.value = false
  }
}

async function searchItems() {
  clearMessages()
  loadingSearch.value = true
  try {
    const response = await api.GET('/items', {
      params: {
        query: {
          code: filters.value.code || undefined,
          desc: filters.value.desc || undefined,
          price_min: filters.value.price_min,
          price_max: filters.value.price_max,
          date_start: filters.value.date_start || undefined,
          date_end: filters.value.date_end || undefined,
        },
      },
    })
    if (!response.data) {
      throw new Error('The server returned no item data.')
    }
    summaries.value = response.data.items
    selectedSummary.value = null
    history.value = []
    priceChart?.destroy()
    priceChart = undefined
  } catch (error) {
    reportError(error)
  } finally {
    loadingSearch.value = false
    await checkServer()
  }
}

async function showHistory(summary: Summary) {
  clearMessages()
  selectedSummary.value = summary
  loadingHistory.value = true
  try {
    const response = await api.GET('/items/{code}/history', {
      params: { path: { code: summary.code } },
    })
    if (!response.data) {
      throw new Error('The server returned no price history.')
    }
    history.value = response.data.items
    await nextTick()
    renderChart()
  } catch (error) {
    reportError(error)
  } finally {
    loadingHistory.value = false
  }
}

function renderChart() {
  if (!chartCanvas.value || !selectedSummary.value) return
  priceChart?.destroy()
  priceChart = new Chart(chartCanvas.value, {
    type: 'line',
    data: {
      labels: history.value.map((item) => item.timestamp),
      datasets: [{
        label: selectedSummary.value.description,
        data: history.value.map((item) => item.price),
        borderColor: '#36765b',
        backgroundColor: 'rgba(54, 118, 91, 0.14)',
        pointRadius: 3,
        tension: 0.2,
        fill: true,
      }],
    },
    options: {
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        y: { title: { display: true, text: 'Price' } },
        x: { title: { display: true, text: 'Date' } },
      },
    },
  })
}

function onImageSelected(event: Event) {
  const input = event.target as HTMLInputElement
  imageFile.value = input.files?.[0] ?? null
  imageItems.value = []
}

async function processImage() {
  if (!imageFile.value) return
  clearMessages()
  processingImage.value = true
  imageItems.value = []
  try {
    const form = new FormData()
    form.append('file', imageFile.value)
    const response = await fetch('/api/v1/receipts/process', { method: 'POST', body: form })
    const body = await response.json() as { items?: ReceiptItem[]; detail?: string }
    if (!response.ok) throw new Error(body.detail ?? 'Could not process this receipt image.')
    imageItems.value = body.items ?? []
    notice.value = imageItems.value.length
      ? `Found ${imageItems.value.length} item${imageItems.value.length === 1 ? '' : 's'}. Review and confirm below.`
      : 'No items were recognized. Try a clearer image.'
  } catch (error) {
    reportError(error)
  } finally {
    processingImage.value = false
    await checkServer()
  }
}

async function confirmReceipt() {
  clearMessages()
  confirmingItems.value = true
  try {
    const response = await api.POST('/receipts/confirm', {
      body: {
        date: receiptDate.value,
        items: imageItems.value.map((item) => ({
          code: item.code,
          description: item.description,
          price: item.price,
          is_unit_price: item.is_unit_price,
        })),
      },
    })
    if (!response.response.ok) {
      throw new Error(response.error?.detail ?? 'Could not save receipt items.')
    }
    const savedMessage = response.data?.message ?? 'Receipt items saved.'
    imageItems.value = []
    imageFile.value = null
    await searchItems()
    notice.value = savedMessage
  } catch (error) {
    reportError(error)
  } finally {
    confirmingItems.value = false
  }
}

function onBatchFilesSelected(event: Event) {
  const input = event.target as HTMLInputElement
  batchFiles.value = Array.from(input.files ?? [])
  batchJob.value = null
}

async function startBatch() {
  if (!batchFiles.value.length) return
  clearMessages()
  processingBatch.value = true
  batchJob.value = null
  const form = new FormData()
  batchFiles.value.forEach((file) => form.append('files', file))
  try {
    const response = await fetch('/api/v1/receipts/batch-jobs', { method: 'POST', body: form })
    const body = await response.json() as BatchJob & { detail?: string }
    if (!response.ok) throw new Error(body.detail ?? 'Could not start batch processing.')
    batchJob.value = body
    await pollBatchJob(body.job_id)
  } catch (error) {
    reportError(error)
    processingBatch.value = false
  }
}

async function pollBatchJob(jobId: string) {
  try {
    while (true) {
      const response = await api.GET('/receipts/batch-jobs/{job_id}', {
        params: { path: { job_id: jobId } },
      })
      if (!response.data) throw new Error(response.error?.detail ?? 'Could not read processing progress.')
      batchJob.value = response.data
      if (response.data.status === 'completed' || response.data.status === 'failed') {
        if (response.data.success) {
          const completedMessage = `Batch complete: ${response.data.total_items} item${response.data.total_items === 1 ? '' : 's'} saved.`
          await searchItems()
          notice.value = completedMessage
        } else {
          errorMessage.value = response.data.message ?? 'Batch processing failed.'
        }
        break
      }
      await new Promise<void>((resolve) => {
        pollingTimer = window.setTimeout(resolve, 1000)
      })
    }
  } catch (error) {
    reportError(error)
  } finally {
    processingBatch.value = false
  }
}

function formatCurrency(value: number) {
  return new Intl.NumberFormat(undefined, { style: 'currency', currency: 'USD' }).format(value)
}

function formatPercent(value: number) {
  return `${value.toFixed(1)}%`
}

function rateClass(value: number) {
  return value <= 2 ? 'rate-low' : value <= 4 ? 'rate-medium' : 'rate-high'
}

onMounted(() => {
  void checkServer()
  void searchItems()
})

onBeforeUnmount(() => {
  priceChart?.destroy()
  if (pollingTimer !== undefined) window.clearTimeout(pollingTimer)
})
</script>

<template>
  <main class="app-shell">
    <header class="topbar">
      <a class="brand" href="#" aria-label="ReceiptReader home">
        <span class="brand-mark">R</span>
        <span>ReceiptReader</span>
      </a>
      <span class="server-status" :class="{ online: serverOnline }">
        <span class="status-dot"></span>
        {{ serverOnline ? 'Server connected' : 'Server unavailable' }}
      </span>
    </header>

    <section class="intro">
      <p class="eyebrow">PERSONAL PRICE HISTORY</p>
      <h1>Know what your groceries really cost.</h1>
      <p>Search purchase history, spot price changes, and turn receipt photos into useful data.</p>
    </section>

    <div v-if="errorMessage" class="alert alert-error" role="alert">{{ errorMessage }}</div>
    <div v-if="notice" class="alert alert-success" role="status">{{ notice }}</div>

    <section class="panel" aria-labelledby="search-title">
      <div class="section-heading">
        <div>
          <p class="eyebrow">YOUR DATABASE</p>
          <h2 id="search-title">Search item history</h2>
        </div>
        <span class="result-count">{{ summaries.length }} results</span>
      </div>

      <form class="filters" @submit.prevent="searchItems">
        <label>
          Item code
          <input v-model="filters.code" name="code" placeholder="Barcode or code" />
        </label>
        <label>
          Description
          <input v-model="filters.desc" name="description" placeholder="Search products" />
        </label>
        <label>
          Minimum price
          <input v-model.number="filters.price_min" name="price-min" type="number" step="0.01" min="0" placeholder="$0.00" />
        </label>
        <label>
          Maximum price
          <input v-model.number="filters.price_max" name="price-max" type="number" step="0.01" min="0" placeholder="$0.00" />
        </label>
        <label>
          Purchased from
          <input v-model="filters.date_start" name="date-start" type="date" />
        </label>
        <label>
          Purchased to
          <input v-model="filters.date_end" name="date-end" type="date" />
        </label>
        <button class="button button-primary search-button" type="submit" :disabled="loadingSearch">
          {{ loadingSearch ? 'Searching…' : 'Search' }}
        </button>
      </form>

      <div v-if="loadingSearch" class="empty-state">Loading your item history…</div>
      <div v-else-if="summaries.length === 0" class="empty-state">
        No matching items yet. Process a receipt photo below to get started.
      </div>
      <div v-else class="table-wrap">
        <table>
          <thead>
            <tr>
              <th>Item</th>
              <th>Code</th>
              <th>Current</th>
              <th>Min</th>
              <th>Max</th>
              <th>Last purchase</th>
              <th>Change</th>
              <th>Lifetime</th>
              <th>1 year</th>
              <th>6 months</th>
              <th>3 months</th>
              <th>1 month</th>
              <th>Purchases</th>
            </tr>
          </thead>
          <tbody>
            <tr
              v-for="summary in summaries"
              :key="summary.code"
              class="summary-row"
              :class="{ selected: selectedSummary?.code === summary.code }"
              tabindex="0"
              @click="showHistory(summary)"
              @keydown.enter="showHistory(summary)"
            >
              <td class="item-name">{{ summary.description }}<small v-if="summary.is_unit_price">Unit price</small></td>
              <td>{{ summary.code }}</td>
              <td class="money">{{ formatCurrency(summary.current_price) }}</td>
              <td class="money">{{ formatCurrency(summary.min_price) }}</td>
              <td class="money">{{ formatCurrency(summary.max_price) }}</td>
              <td>{{ summary.timestamp_last }}</td>
              <td class="money">{{ formatCurrency(summary.latest_price_change) }}</td>
              <td><span class="rate" :class="rateClass(summary.lifetime_inflation_rate)">{{ formatPercent(summary.lifetime_inflation_rate) }}</span></td>
              <td><span class="rate" :class="rateClass(summary.one_year_inflation_rate)">{{ formatPercent(summary.one_year_inflation_rate) }}</span></td>
              <td><span class="rate" :class="rateClass(summary.six_month_inflation_rate)">{{ formatPercent(summary.six_month_inflation_rate) }}</span></td>
              <td><span class="rate" :class="rateClass(summary.three_month_inflation_rate)">{{ formatPercent(summary.three_month_inflation_rate) }}</span></td>
              <td><span class="rate" :class="rateClass(summary.one_month_inflation_rate)">{{ formatPercent(summary.one_month_inflation_rate) }}</span></td>
              <td>{{ summary.times_purchased }}</td>
            </tr>
          </tbody>
        </table>
      </div>

      <div v-if="selectedSummary" class="history-panel">
        <div class="history-heading">
          <div>
            <p class="eyebrow">PRICE HISTORY</p>
            <h3>{{ selectedSummary.description }}</h3>
          </div>
          <span v-if="loadingHistory" class="muted">Loading history…</span>
        </div>
        <div v-if="history.length" class="chart-container">
          <canvas ref="chartCanvas" aria-label="Item price history chart"></canvas>
        </div>
        <p v-else-if="!loadingHistory" class="muted">No price history available for this item.</p>
      </div>
    </section>

    <section class="panel process-panel" aria-labelledby="process-title">
      <div class="section-heading">
        <div>
          <p class="eyebrow">ADD RECEIPTS</p>
          <h2 id="process-title">Process receipt images</h2>
        </div>
      </div>
      <div class="processing-grid">
        <form class="upload-card" @submit.prevent="processImage">
          <h3>One receipt</h3>
          <p>Review recognized items before saving them.</p>
          <label class="file-picker">
            <span>{{ imageFile?.name ?? 'Choose a receipt image' }}</span>
            <input type="file" accept="image/jpeg,image/png,image/heic,.heic" @change="onImageSelected" />
          </label>
          <button class="button button-primary" type="submit" :disabled="!imageFile || processingImage">
            {{ processingImage ? 'Processing…' : 'Read receipt' }}
          </button>

          <div v-if="imageItems.length" class="review-list">
            <h4>Review {{ imageItems.length }} recognized items</h4>
            <div v-for="(item, index) in imageItems" :key="`${item.code}-${index}`" class="review-item">
              <input v-model="item.description" :aria-label="`Item ${index + 1} description`" />
              <input v-model="item.code" :aria-label="`Item ${index + 1} code`" />
              <input v-model.number="item.price" type="number" min="0" step="0.01" :aria-label="`Item ${index + 1} price`" />
            </div>
            <label>
              Receipt date
              <input v-model="receiptDate" type="date" required />
            </label>
            <button class="button button-primary" type="button" :disabled="!canConfirm" @click="confirmReceipt">
              {{ confirmingItems ? 'Saving…' : 'Confirm and save' }}
            </button>
          </div>
        </form>

        <form class="upload-card" @submit.prevent="startBatch">
          <h3>Multiple receipts</h3>
          <p>Upload a group of images. Parsed items are saved automatically.</p>
          <label class="file-picker">
            <span>{{ batchFiles.length ? `${batchFiles.length} image${batchFiles.length === 1 ? '' : 's'} selected` : 'Choose multiple images' }}</span>
            <input type="file" accept="image/jpeg,image/png,image/heic,.heic" multiple @change="onBatchFilesSelected" />
          </label>
          <button class="button button-secondary" type="submit" :disabled="batchFiles.length === 0 || processingBatch">
            {{ processingBatch ? 'Processing…' : 'Process selected images' }}
          </button>
          <div v-if="batchJob" class="job-status" role="status">
            <div class="job-line">
              <strong>{{ batchJob.status === 'completed' ? 'Complete' : batchJob.status === 'failed' ? 'Failed' : 'Processing receipts' }}</strong>
              <span>{{ batchJob.processed_images }} / {{ batchJob.total_images }}</span>
            </div>
            <progress :value="batchJob.processed_images" :max="Math.max(batchJob.total_images, 1)"></progress>
            <span class="muted">{{ batchJob.current_image || batchJob.message || 'Waiting for processing…' }}</span>
          </div>
        </form>
      </div>
    </section>

    <footer>ReceiptReader · Your receipt data stays on your server.</footer>
  </main>
</template>
