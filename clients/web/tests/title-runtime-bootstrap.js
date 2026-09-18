// Existing interaction suites begin from the shipped seed through the real title.
(function () {
  const timer=setInterval(()=>{
    const button=document.querySelector('[data-adventure-action="continue"][data-slot="1"]');
    if(button && !button.disabled && document.querySelector('#adventureDialog').open) {
      clearInterval(timer);button.click();
    }
  },100);
})();
