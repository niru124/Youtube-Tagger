chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
  if (changeInfo.status === 'complete' && tab.url && tab.url.includes('youtube.com/watch')) {
    browser.action.enable(tab.id);
  } else {
    chrome.action.disable(tabId);
  }
});
