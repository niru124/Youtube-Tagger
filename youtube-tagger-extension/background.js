chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
  if (changeInfo.status === 'complete' && tab.url && tab.url.includes('youtube.com/watch')) {
    chrome.action.enable(tabId);
  } else {
    chrome.action.disable(tabId);
  }
});