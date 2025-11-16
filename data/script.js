let cardData = [];
const tableBody = document.getElementById('cardTable').getElementsByTagName('tbody')[0];
const userTableBody = document.getElementById('userTable').getElementsByTagName('tbody')[0];
const lastReadCardsTableBody = document.getElementById('lastReadCardsTable').getElementsByTagName('tbody')[0];
const importExportArea = document.getElementById('importExportArea');

function updateTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            tableBody.innerHTML = '';
            data.forEach((card, index) => {
                cardData.push(card);
                let row = tableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellBitLength = row.insertCell(1);
                let cellFacilityCode = row.insertCell(2);
                let cellCardNumber = row.insertCell(3);
                let cellHexData = row.insertCell(4);
                let cellRawData = row.insertCell(5);

                cellIndex.innerHTML = index + 1;
                cellBitLength.innerHTML = card.bitCount;
                cellFacilityCode.innerHTML = card.facilityCode;
                cellCardNumber.innerHTML = card.cardNumber;
                cellHexData.innerHTML = `<a href="#" onclick="copyToClipboard('${card.hexCardData}')">${card.hexCardData}</a>`;
                cellRawData.innerHTML = card.rawCardData;
            });
        })
        .catch(error => console.error('Error fetching card data:', error));
}

function updateUserTable() {
    fetch('/getUsers')
        .then(response => response.json())
        .then(data => {
            userTableBody.innerHTML = '';
            data.forEach((user, index) => {
                let row = userTableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellFacilityCode = row.insertCell(1);
                let cellCardNumber = row.insertCell(2);
                let cellName = row.insertCell(3);
                let cellAction = row.insertCell(4);

                cellIndex.innerHTML = index + 1;
                cellFacilityCode.innerHTML = user.facilityCode;
                cellCardNumber.innerHTML = user.cardNumber;
                cellName.innerHTML = user.name;
                cellAction.innerHTML = '<button onclick="deleteCard(' + index + ')">Delete</button>';
            });

            // Add input row at the bottom of the table
            let inputRow = userTableBody.insertRow();
            inputRow.className = 'inputRow';
            let cellIndex = inputRow.insertCell(0);
            let cellFacilityCode = inputRow.insertCell(1);
            let cellCardNumber = inputRow.insertCell(2);
            let cellName = inputRow.insertCell(3);
            let cellAction = inputRow.insertCell(4);

            cellFacilityCode.innerHTML = '<input type="number" id="newFacilityCode">';
            cellCardNumber.innerHTML = '<input type="number" id="newCardNumber">';
            cellName.innerHTML = '<input type="text" id="newName">';
            cellAction.innerHTML = '<button onclick="addCard()">Save</button>';
        })
        .catch(error => console.error('Error fetching user data:', error));
}

function updateLastReadCardsTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            lastReadCardsTableBody.innerHTML = '';
            const last10Cards = data.slice(-10);
            last10Cards.forEach((card, index) => {
                let row = lastReadCardsTableBody.insertRow();
                if (card.status === "Authorized") {
                    row.classList.add("authorized");
                } else if (card.status === "Unauthorized") {
                    row.classList.add("unauthorized");
                }
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellDetails = row.insertCell(2);

                cellIndex.innerHTML = index + 1;
                cellStatus.innerHTML = card.status;
                cellDetails.innerHTML = card.details;
            });

            // Add empty rows if there are less than 10 entries
            for (let i = last10Cards.length; i < 10; i++) {
                let row = lastReadCardsTableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellStatus = row.insertCell(1);
                let cellDetails = row.insertCell(2);

                cellIndex.innerHTML = i + 1;
                cellStatus.innerHTML = "";
                cellDetails.innerHTML = "";
            }
        })
        .catch(error => console.error('Error fetching last read card data:', error));
}

function addCard() {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;

    fetch(`/addCard?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${name}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('Card added successfully');
            } else {
                alert('Failed to add card');
            }
        })
        .catch(error => console.error('Error adding card:', error));
}

function deleteCard(index) {
    fetch(`/deleteCard?index=${index}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('Card deleted successfully');
            } else {
                alert('Failed to delete card');
            }
        })
        .catch(error => console.error('Error deleting card:', error));
}

function showSection(section) {
    document.getElementById('lastRead').classList.add('hidden');
    document.getElementById('ctfMode').classList.add('hidden');
    document.getElementById('settings').classList.add('hidden');
    document.getElementById(section).classList.remove('hidden');
}

function toggleCollapsible() {
    const content = document.querySelector(".contentCollapsible");
    content.style.display = content.style.display === "block" ? "none" : "block";
}

function toggleWelcomeMessage() {
    const welcomeMessage = document.getElementById('welcomeMessage').value;
    const customMessageDiv = document.getElementById('customMessageDiv');
    if (welcomeMessage === 'custom') {
        customMessageDiv.removeAttribute("hidden");
    } else {
        customMessageDiv.setAttribute("hidden", "hidden");
    }
}

function updateSettingsUI(settings) {
    document.getElementById('modeSelect').value = settings.mode;
    document.getElementById('timeoutSelect').value = settings.displayTimeout;
    document.getElementById('ap_ssid').value = settings.apSsid;
    document.getElementById('ap_passphrase').value = settings.apPassphrase;
    document.getElementById('ssid_hidden').checked = settings.ssidHidden;
    document.getElementById('ap_channel').value = settings.apChannel;
    document.getElementById('welcomeMessage').value = settings.welcomeMessage;
    document.getElementById('customMessage').value = settings.customMessage;
    document.getElementById('ledValid').value = settings.ledValid;
    document.getElementById('spkOnValid').value = settings.spkOnValid;
    document.getElementById('spkOnInvalid').value = settings.spkOnInvalid;
    //toggleWifiSettings();
    toggleWelcomeMessage();
}

function saveSettings() {
    const mode = document.getElementById('modeSelect').value;
    const timeout = document.getElementById('timeoutSelect').value;
    const apSsid = document.getElementById('ap_ssid').value;
    const apPassphrase = document.getElementById('ap_passphrase').value;
    const ssidHidden = document.getElementById('ssid_hidden').checked;
    const apChannel = document.getElementById('ap_channel').value;
    const welcomeMessage = document.getElementById('welcomeMessage').value;
    const customMessage = document.getElementById('customMessage').value;
    const ledValid = document.getElementById('ledValid').value;
    const spkOnValid = document.getElementById('spkOnValid').value;
    const spkOnInvalid = document.getElementById('spkOnInvalid').value;

    let settings = {
        mode: mode,
        displayTimeout: parseInt(timeout, 10),
        apSsid: apSsid,
        apPassphrase: apPassphrase,
        ssidHidden: ssidHidden,
        apChannel: parseInt(apChannel),
        welcomeMessage: welcomeMessage,
        customMessage: customMessage,
        ledValid: parseInt(ledValid),
        spkOnValid: parseInt(spkOnValid),
        spkOnInvalid: parseInt(spkOnInvalid)
    };

    fetch('/saveSettings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(settings)
    })
        .then(response => {
            if (response.ok) {
                alert('Settings saved successfully');
            } else {
                alert('Failed to save settings');
            }
        })
        .catch(error => console.error('Error saving settings:', error));
}

function fetchSettings() {
    fetch('/getSettings')
        .then(response => response.json())
        .then(data => {
            updateSettingsUI(data);
        })
        .catch(error => console.error('Error fetching settings:', error));
}

window.onload = fetchSettings;
function exportData() {
    fetch('/getUsers')
        .then(response => response.json())
        .then(data => {
            const dataString = JSON.stringify(data);
            importExportArea.value = dataString;
            importExportArea.select();
            document.execCommand('copy');
            alert('Data exported to clipboard');
        })
        .catch(error => console.error('Error exporting data:', error));
}

function importData() {
    const dataString = importExportArea.value;
    try {
        const data = JSON.parse(dataString);
        if (Array.isArray(data)) {
            data.forEach(card => {
                const facilityCode = card.facilityCode;
                const cardNumber = card.cardNumber;
                const name = card.name;

                fetch(`/addCard?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${name}`)
                    .then(response => {
                        if (!response.ok) {
                            throw new Error('Failed to add card');
                        }
                    })
                    .catch(error => console.error('Error adding card:', error));
            });
            updateUserTable();
            alert('Data imported successfully');
        } else {
            alert('Invalid data format');
        }
    } catch (error) {
        alert('Invalid JSON format');
    }
}

function copyToClipboard(text) {
    const tempInput = document.createElement('input');
    tempInput.style.position = 'absolute';
    tempInput.style.left = '-9999px';
    tempInput.value = text;
    document.body.appendChild(tempInput);
    tempInput.select();
    document.execCommand('copy');
    document.body.removeChild(tempInput);
}

setInterval(updateTable, 5000);
setInterval(updateLastReadCardsTable, 5000);
updateTable();
updateUserTable();
updateLastReadCardsTable();