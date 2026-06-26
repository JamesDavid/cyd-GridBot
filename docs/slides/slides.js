/* Minimal, dependency-free slide engine for the GridBot classroom decks.
   Keys: → / Space / PageDn = next · ← / PageUp = back · Home/End = first/last
         F = fullscreen · N = toggle teacher notes. Click the on-screen arrows too. */
(function () {
  var deck = document.getElementById('deck');
  var slides = Array.prototype.slice.call(deck.querySelectorAll('.slide'));
  var counter = document.getElementById('counter');
  var prog = document.getElementById('prog');
  var noteEl = document.getElementById('note');
  var i = 0, notesOn = true;

  function clamp(n) { return Math.max(0, Math.min(slides.length - 1, n)); }
  function show(n) {
    i = clamp(n);
    for (var k = 0; k < slides.length; k++) slides[k].classList.toggle('active', k === i);
    counter.textContent = (i + 1) + ' / ' + slides.length;
    prog.style.width = (slides.length > 1 ? (i / (slides.length - 1) * 100) : 100) + '%';
    var note = slides[i].getAttribute('data-note') || '';
    noteEl.innerHTML = note ? '<b>👩‍🏫 Teacher:</b> ' + note : '';
    noteEl.style.display = (note && notesOn) ? 'block' : 'none';
    try { history.replaceState(null, '', '#' + (i + 1)); } catch (e) {}
    deck.scrollTop = 0; slides[i].scrollTop = 0;
  }
  function next() { show(i + 1); }
  function prev() { show(i - 1); }
  function toggleFs() {
    if (!document.fullscreenElement) { (document.documentElement.requestFullscreen || function(){})(); }
    else if (document.exitFullscreen) { document.exitFullscreen(); }
  }

  document.addEventListener('keydown', function (e) {
    var k = e.key;
    if (k === 'ArrowRight' || k === 'PageDown' || k === ' ' || k === 'Spacebar') { e.preventDefault(); next(); }
    else if (k === 'ArrowLeft' || k === 'PageUp') { e.preventDefault(); prev(); }
    else if (k === 'Home') { e.preventDefault(); show(0); }
    else if (k === 'End') { e.preventDefault(); show(slides.length - 1); }
    else if (k === 'f' || k === 'F') { toggleFs(); }
    else if (k === 'n' || k === 'N') { notesOn = !notesOn; show(i); }
  });

  var b;
  if ((b = document.getElementById('next'))) b.onclick = next;
  if ((b = document.getElementById('prev'))) b.onclick = prev;
  if ((b = document.getElementById('fs'))) b.onclick = toggleFs;

  var start = parseInt((location.hash || '').replace('#', ''), 10);
  show(isNaN(start) ? 0 : start - 1);
})();
