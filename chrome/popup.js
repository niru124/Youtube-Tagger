document.addEventListener('DOMContentLoaded', () => {
  const videoUrlInput = document.getElementById('videoUrl');
  const tagsInput = document.getElementById('tags');
  const submitButton = document.getElementById('submitTags');
  const statusDiv = document.getElementById('status');
  const suggestionsDiv = document.getElementById('suggestions');

  let allExistingTopics = [];
  let currentVideoId = null;

  // Function to fetch existing topics for the current video
  async function fetchExistingTopics(videoId) {
    try {
      const response = await fetch(`http://localhost:8000/videos/${videoId}/topics`);
      if (response.ok) {
        const data = await response.json();
        allExistingTopics = data.topics ? data.topics.map(topic => topic && topic.topic_name ? topic.topic_name : '').filter(name => name.length > 0) : [];
      } else {
        console.error('Failed to fetch existing topics:', response.statusText);
        allExistingTopics = [];
      }
    } catch (error) {
      console.error('Error fetching existing topics:', error);
      allExistingTopics = [];
    }
  }

  // Get current tab URL and display it
  chrome.tabs.query({ active: true, currentWindow: true }, async (tabs) => {
    console.log('chrome.tabs.query result:', tabs);
    const currentTab = tabs[0];
    if (currentTab && currentTab.url && currentTab.url.includes('youtube.com/watch')) {
      console.log('Current YouTube URL:', currentTab.url);
      videoUrlInput.value = currentTab.url;
      const videoIdMatch = currentTab.url.match(/[?&]v=([^&]+)/);
      if (videoIdMatch) {
        currentVideoId = videoIdMatch[1];
        // Inject script to get video title
        chrome.scripting.executeScript({
          target: { tabId: currentTab.id },
          func: () => document.title
        }, async (injectionResults) => {
          const videoTitle = injectionResults[0].result.replace(' - YouTube', '');
          await ensureVideoExists(currentVideoId, videoTitle);
          await fetchExistingTopics(currentVideoId);
        });
      }
    } else {
      videoUrlInput.value = 'Not a YouTube watch page.';
      submitButton.disabled = true;
    }
  });

  // New function to ensure video exists in the database
  async function ensureVideoExists(videoId, videoTitle) {
    try {
      // First, check if the video already exists
      const checkResponse = await fetch(`http://localhost:8000/videos/${videoId}`);
      if (checkResponse.ok) {
        console.log(`Video ${videoId} already exists in the database.`);
        return; // Video already exists, no need to add
      } else if (checkResponse.status === 404) {
        console.log(`Video ${videoId} not found, adding it to the database.`);
        // If not found, add the video
        const addResponse = await fetch(`http://localhost:8000/videos`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            id: `https://www.youtube.com/watch?v=${videoId}`,
            title: videoTitle
          })
        });

        if (addResponse.ok) {
          console.log(`Video ${videoId} added successfully.`);
        } else {
          const errorData = await addResponse.json();
          console.error(`Failed to add video ${videoId}:`, errorData.error || addResponse.statusText);
          statusDiv.textContent = `Error adding video: ${errorData.error || addResponse.statusText}`;
          statusDiv.className = 'error';
        }
      } else {
        console.error(`Error checking video ${videoId} existence:`, checkResponse.statusText);
        statusDiv.textContent = `Error checking video existence: ${checkResponse.statusText}`;
        statusDiv.className = 'error';
      }
    } catch (error) {
      console.error(`Network error while ensuring video ${videoId} exists:`, error);
      statusDiv.textContent = `Network error: ${error.message}`;
      statusDiv.className = 'error';
    }
  }

  // Autocompletion logic
  tagsInput.addEventListener('input', () => {
    const inputValue = tagsInput.value;
    const lastCommaIndex = inputValue.lastIndexOf(',');
    const currentTag = (lastCommaIndex !== -1 ? inputValue.substring(lastCommaIndex + 1) : inputValue).trim().toLowerCase();

    suggestionsDiv.innerHTML = '';
    suggestionsDiv.style.display = 'none';

    if (currentTag.length > 0 && allExistingTopics.length > 0) {
      const filteredSuggestions = allExistingTopics.filter(topic =>
        topic.toLowerCase().startsWith(currentTag)
      );

      if (filteredSuggestions.length > 0) {
        suggestionsDiv.style.display = 'block';
        filteredSuggestions.forEach(suggestion => {
          const suggestionItem = document.createElement('div');
          suggestionItem.textContent = suggestion;
          suggestionItem.style.padding = '5px';
          suggestionItem.style.cursor = 'pointer';
          suggestionItem.style.borderBottom = '1px solid #eee';
          suggestionItem.onmouseover = () => suggestionItem.style.backgroundColor = '#f0f0f0';
          suggestionItem.onmouseout = () => suggestionItem.style.backgroundColor = 'white';
          suggestionItem.onclick = () => {
            const newTags = lastCommaIndex !== -1
              ? inputValue.substring(0, lastCommaIndex + 1) + suggestion + ', '
              : suggestion + ', ';
            tagsInput.value = newTags;
            suggestionsDiv.style.display = 'none';
            tagsInput.focus(); // Keep focus on the input
          };
          suggestionsDiv.appendChild(suggestionItem);
        });
      }
    }
  });

  submitButton.addEventListener('click', async () => {
    const videoUrl = videoUrlInput.value;
    const tags = tagsInput.value.split(',').map(tag => tag.trim()).filter(tag => tag.length > 0);

    if (!videoUrl || videoUrl === 'Not a YouTube watch page.' || tags.length === 0) {
      statusDiv.textContent = 'Please enter a valid YouTube URL and at least one tag.';
      statusDiv.className = 'error';
      return;
    }

    if (!currentVideoId) {
      statusDiv.textContent = 'Could not extract video ID from the URL.';
      statusDiv.className = 'error';
      return;
    }

    const userId = localStorage.getItem('user_id'); // Assuming userId is stored in localStorage
    if (!userId) {
      statusDiv.textContent = 'Error: User ID not found in local storage. Please log in.';
      statusDiv.className = 'error';
      return;
    }

    statusDiv.textContent = 'Submitting tags...';
    statusDiv.className = '';

    try {
      let successMessages = [];
      let errorMessages = [];

      for (const tag of tags) {
        const response = await fetch(`http://localhost:8000/videos/${currentVideoId}/topics`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            name: tag,
            desired_vote: 1, // Always upvote for this extension
            user_id: userId
          })
        });

        if (response.ok) {
          const data = await response.json();
          successMessages.push(`Tag '${tag}': ${data.message || 'Success'}`);
        } else {
          const errorData = await response.json();
          errorMessages.push(`Tag '${tag}': ${errorData.error || response.statusText}`);
        }
      }

      if (successMessages.length > 0) {
        statusDiv.textContent = successMessages.join('; ');
        statusDiv.className = 'success';
        tagsInput.value = ''; // Clear tags after successful submission
        // Re-fetch topics to update autocompletion list with newly added tags
        await fetchExistingTopics(currentVideoId);
      }
      if (errorMessages.length > 0) {
        statusDiv.textContent += (successMessages.length > 0 ? '; ' : '') + `Errors: ${errorMessages.join('; ')}`;
        statusDiv.className = (successMessages.length > 0 ? 'partial-success' : 'error');
      }
      if (successMessages.length === 0 && errorMessages.length === 0) {
        statusDiv.textContent = 'No tags processed.';
        statusDiv.className = '';
      }

    } catch (error) {
      statusDiv.textContent = `Error: ${error.message}`;
      statusDiv.className = 'error';
      console.error('Error submitting tags:', error);
    }
  });
});
