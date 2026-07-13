(function () {
  async function loadStatus() {
    const p = document.getElementById('status');
    if (!p) {
      return;
    }
    try {
      const r = await fetch('/api/status');
      p.textContent = await r.text();
    } catch (e) {
      p.textContent = 'status request failed';
    }
  }

  /* Setup form uses native POST; full HTML response from /api/provision replaces the page. */
  loadStatus();
})();
