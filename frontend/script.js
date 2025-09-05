const API_BASE = 'http://localhost:8000';

// Function to generate a simple UUID-like string for user_id
function generateUserId() {
    return 'user-' + Math.random().toString(36).substring(2, 11);
}

// Function to get or create a user ID
function getOrCreateUserId() {
    let userId = localStorage.getItem('user_id');
    if (!userId) {
        userId = generateUserId();
        localStorage.setItem('user_id', userId);
    }
    return userId;
}

// Call this once when your application loads
const currentUserId = getOrCreateUserId();
console.log('Current User ID:', currentUserId);

function extractVideoId(url) {
    const match = url.match(/(?:youtube\.com\/watch\?v=|youtu\.be\/)([a-zA-Z0-9_-]{11})/);
    return match ? match[1] : url; // If no match, assume it's already the ID
}

async function addVideo() {
    const url = document.getElementById('video-url').value;
    const title = document.getElementById('video-title').value;
    const resultDiv = document.getElementById('add-result');
    resultDiv.innerHTML = ''; // Clear previous results

    try {
        const response = await fetch(`${API_BASE}/videos`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ id: url, title })
        });

        if (response.status === 204) {
            resultDiv.innerHTML = '<pre>Success (no content)</pre>';
        } else if (response.ok) {
            const result = await response.json();
            resultDiv.innerHTML = `<pre>${JSON.stringify(result, null, 2)}</pre>`;
        } else {
            const errorText = await response.text();
            resultDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        resultDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}

async function getTopics() {
    const videoInput = document.getElementById('video-id').value;
    const videoId = extractVideoId(videoInput);
    const topicsListDiv = document.getElementById('topics-list');
    topicsListDiv.innerHTML = ''; // Clear previous results

    try {
        const response = await fetch(`${API_BASE}/videos/${videoId}/topics`);
        if (response.ok) {
            const result = await response.json();
            if (result.topics && result.topics.length > 0) {
                topicsListDiv.innerHTML = result.topics.map(t => `<div class="topic">${t.topic_name} (Votes: ${t.total_votes})</div>`).join('');
            } else {
                topicsListDiv.innerHTML = '<div class="topic">No topics found for this video.</div>';
            }
        } else {
            const errorText = await response.text();
            topicsListDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        topicsListDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}

async function voteTopic() {
    const videoInput = document.getElementById('vote-video-id').value;
    const videoId = extractVideoId(videoInput);
    const topicName = document.getElementById('topic-name').value;
    const topicId = document.getElementById('topic-id-vote').value;
    const desiredVote = document.getElementById('vote-type').value;
    const voteResultDiv = document.getElementById('vote-result');
    voteResultDiv.innerHTML = ''; // Clear previous results

    const body = { desired_vote: parseInt(desiredVote) };
    body.user_id = currentUserId; // Use the automatically managed user ID

    if (topicId) {
        body.topic_id = parseInt(topicId);
    } else if (topicName) {
        body.name = topicName;
    } else {
        voteResultDiv.innerHTML = '<pre>Error: Either Topic Name or Topic ID is required.</pre>';
        return;
    }

    try {
        const response = await fetch(`${API_BASE}/videos/${videoId}/topics`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        if (response.ok) {
            const result = await response.json();
            voteResultDiv.innerHTML = `<pre>${JSON.stringify(result, null, 2)}</pre>`;
        } else {
            const errorText = await response.text();
            voteResultDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        voteResultDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}

async function getSimilar() {
    const videoInput = document.getElementById('similar-video-id').value;
    const videoId = extractVideoId(videoInput);
    const similarListDiv = document.getElementById('similar-list');
    similarListDiv.innerHTML = ''; // Clear previous results

    try {
        const response = await fetch(`${API_BASE}/videos/${videoId}/similar`);
        if (response.ok) {
            const result = await response.json();
            if (result && result.length > 0) {
                similarListDiv.innerHTML = result.map(v => `<div class="similar-video"><a href="https://www.youtube.com/watch?v=${v.video_id}" target="_blank">${v.title || v.video_id}</a> (${v.shared_topics_count} shared topics)</div>`).join('');
            } else {
                similarListDiv.innerHTML = '<div class="similar-video">No similar videos found.</div>';
            }
        } else {
            const errorText = await response.text();
            similarListDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        similarListDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}

async function getUserStats() {
    const userId = document.getElementById('stats-user-id').value; // Corrected ID
    const userStatsResultDiv = document.getElementById('user-stats-result');
    userStatsResultDiv.innerHTML = ''; // Clear previous results

    try {
        const response = await fetch(`${API_BASE}/users/${userId}/stats`);
        if (response.ok) {
            const result = await response.json();
            userStatsResultDiv.innerHTML = `<pre>${JSON.stringify(result, null, 2)}</pre>`;
        } else {
            const errorText = await response.text();
            userStatsResultDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        userStatsResultDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}

async function getContributions() {
    const contributionsListDiv = document.getElementById('contributions-list');
    contributionsListDiv.innerHTML = ''; // Clear previous results

    try {
        const response = await fetch(`${API_BASE}/users/contributions`);
        if (response.ok) {
            const result = await response.json();
            if (result && result.length > 0) {
                contributionsListDiv.innerHTML = result.map(u => `<div class="contribution">${u.username || u.id}: ${u.contributions_count} contributions</div>`).join('');
            } else {
                contributionsListDiv.innerHTML = '<div class="contribution">No user contributions found.</div>';
            }
        } else {
            const errorText = await response.text();
            contributionsListDiv.innerHTML = `<pre>Error: ${response.status} ${response.statusText}\n${errorText}</pre>`;
        }
    } catch (error) {
        contributionsListDiv.innerHTML = `<pre>Error: ${error.message}</pre>`;
    }
}
