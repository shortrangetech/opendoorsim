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
            cardData = [];
            data.forEach((card, index) => {
                cardData.push(card);
                let row = tableBody.insertRow();
                let cellIndex = row.insertCell(0);
                let cellBitLength = row.insertCell(1);
                let cellFacilityCode = row.insertCell(2);
                let cellCardNumber = row.insertCell(3);
                let cellRawData = row.insertCell(4);
                let cellHexData = row.insertCell(5);
                let cellPadCount = row.insertCell(6);

                cellIndex.innerHTML = index + 1;
                cellBitLength.innerHTML = card.bitCount;
                cellFacilityCode.innerHTML = card.facilityCode;
                cellCardNumber.innerHTML = card.cardNumber;
                cellRawData.innerHTML = `<a href="#" onclick="copyToClipboard('${card.rawCardData}')">${card.rawCardData}</a>`;
                cellHexData.innerHTML = card.hexData || 'N/A';
                cellPadCount.innerHTML = card.padCount === 0 ? 'None' : card.padCount;
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
                let cellFlag = row.insertCell(4);
                let cellAction = row.insertCell(5);

                cellIndex.innerHTML = index + 1;
                cellFacilityCode.innerHTML = user.facilityCode;
                cellCardNumber.innerHTML = user.cardNumber;
                cellName.innerHTML = user.name;
                cellFlag.innerHTML = user.flag || '';
                cellAction.innerHTML = '<button onclick="editUser(' + index + ')">Edit</button> <button onclick="deleteUser(' + index + ')">Delete</button>';
            });

            // Add input row at the bottom of the table
            let inputRow = userTableBody.insertRow();
            inputRow.className = 'inputRow';
            let cellIndex = inputRow.insertCell(0);
            let cellFacilityCode = inputRow.insertCell(1);
            let cellCardNumber = inputRow.insertCell(2);
            let cellName = inputRow.insertCell(3);
            let cellFlag = inputRow.insertCell(4);
            let cellAction = inputRow.insertCell(5);

            cellFacilityCode.innerHTML = '<input type="number" id="newFacilityCode">';
            cellCardNumber.innerHTML = '<input type="number" id="newCardNumber">';
            cellName.innerHTML = '<input type="text" id="newName">';
            cellFlag.innerHTML = '<input type="text" id="newFlag">';
            cellAction.innerHTML = '<button id="saveUserButton" onclick="addUser()">Save</button> <button id="cancelEditButton" style="display:none;" onclick="cancelEdit()">Cancel</button>';
        })
        .catch(error => console.error('Error fetching user data:', error));
}

function updateLastReadCardsTable() {
    fetch('/getCards')
        .then(response => response.json())
        .then(data => {
            lastReadCardsTableBody.innerHTML = '';
            // show newest card first: reverse the last 10 so newest is index 0
            const last10Cards = data.slice(-10).reverse();
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

function addUser() {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;

    fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${name}&flag=${encodeURIComponent(flag)}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('User added successfully');
            } else {
                alert('Failed to add user');
            }
        })
        .catch(error => console.error('Error adding card:', error));
}

function deleteUser(index) {
    fetch(`/deleteUser?index=${index}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('User deleted successfully');
            } else {
                alert('Failed to delete user');
            }
        })
        .catch(error => console.error('Error deleting user:', error));
}

function showSection(section) {
    document.getElementById('lastRead').classList.add('hidden');
    document.getElementById('ctfMode').classList.add('hidden');
    document.getElementById('settings').classList.add('hidden');
    document.getElementById(section).classList.remove('hidden');
}

// When switching to the CTF Mode tab, refresh settings so indicator is up-to-date
function showSectionWithRefresh(section) {
    showSection(section);
    if (section === 'ctfMode') {
        fetchSettings();
    }
}

function toggleCollapsible() {
    const content = document.querySelector(".contentCollapsible");
    content.style.display = content.style.display === "block" ? "none" : "block";
}

function updateSettingsUI(settings) {
    // Support both backend keys (device_mode, display_timeout, ap_ssid, etc.)
    const mode = settings.device_mode || settings.mode || '';
    const displayTimeout = settings.display_timeout || settings.displayTimeout || '';
    const apSsid = settings.ap_ssid || settings.apSsid || '';
    const apPass = settings.ap_pwd || settings.apPassphrase || '';
    const ssidHidden = (settings.ssid_hidden !== undefined) ? settings.ssid_hidden : settings.ssidHidden;
    const customMessage = settings.custom_message || settings.customMessage || '';
    const ledValid = settings.led_valid || settings.ledValid || 1;
    const activeDisplayType = settings.active_display_type || settings.activeDisplayType || '';
    const enableTamperDetect = (settings.enable_tamper_detect !== undefined) ? settings.enable_tamper_detect : settings.enableTamperDetect;
    const maxBits = settings.max_bits || settings.maxBits || '';
    const wiegandWait = settings.wiegand_wait_time || settings.wiegandWaitTime || '';
    const apMode = (settings.ap_mode !== undefined) ? settings.ap_mode : settings.apMode;

    if (document.getElementById('modeSelect')) document.getElementById('modeSelect').value = (mode || '').toString().toLowerCase();
    // Display mode in header as ALL CAPS
    const modeValueEl = document.getElementById('modeValue');
    if (modeValueEl) {
        modeValueEl.textContent = (mode || '').toString().toUpperCase();
    }
    if (document.getElementById('timeoutSelect')) document.getElementById('timeoutSelect').value = displayTimeout;
    if (document.getElementById('ap_ssid')) document.getElementById('ap_ssid').value = apSsid;
    if (document.getElementById('ap_pwd')) document.getElementById('ap_pwd').value = apPass;
    if (document.getElementById('ssid_hidden') && typeof ssidHidden !== 'undefined') document.getElementById('ssid_hidden').checked = ssidHidden;
    // AP channel control removed from UI
    if (document.getElementById('customMessage')) document.getElementById('customMessage').value = customMessage;
    if (document.getElementById('ledValid')) document.getElementById('ledValid').value = ledValid;
    if (document.getElementById('activeDisplayType')) document.getElementById('activeDisplayType').value = activeDisplayType;
    if (document.getElementById('enable_tamper_detect')) document.getElementById('enable_tamper_detect').checked = enableTamperDetect;
    if (document.getElementById('max_bits')) document.getElementById('max_bits').value = maxBits;
    if (document.getElementById('wiegand_wait_time')) document.getElementById('wiegand_wait_time').value = wiegandWait;
}

function updateCTFIndicator(settings) {
    const mode = (settings.device_mode || settings.mode || '').toString().toLowerCase();
    // Require strict 'ctf' mode value from backend/frontend
    const isCTFOn = (mode === 'ctf');
    const statusEl = document.getElementById('ctfStatus');
    const hintEl = document.getElementById('ctfHint');
    if (statusEl) statusEl.textContent = isCTFOn ? 'ON' : 'OFF';
    if (hintEl) hintEl.style.display = isCTFOn ? 'none' : 'block';
}

function saveSettings() {
    const mode = document.getElementById('modeSelect').value.toString().toLowerCase();
    const timeout = document.getElementById('timeoutSelect').value;
    const apSsid = document.getElementById('ap_ssid').value;
    const apPassphrase = document.getElementById('ap_pwd').value;
    const ssidHidden = document.getElementById('ssid_hidden').checked ? 1 : 0;
    const customMessage = document.getElementById('customMessage').value;
    const ledValid = document.getElementById('ledValid').value;
    const activeDisplayType = document.getElementById('activeDisplayType') ? parseInt(document.getElementById('activeDisplayType').value,10) : 1;
    const enableTamperDetect = document.getElementById('enable_tamper_detect') ? (document.getElementById('enable_tamper_detect').checked ? true : false) : false;
    const maxBits = document.getElementById('max_bits') ? parseInt(document.getElementById('max_bits').value,10) : undefined;
    const wiegandWait = document.getElementById('wiegand_wait_time') ? parseInt(document.getElementById('wiegand_wait_time').value,10) : undefined;

    let settings = {
        device_mode: mode,
        display_timeout: parseInt(timeout, 10),
        ap_ssid: apSsid,
        ap_pwd: apPassphrase,
        ssid_hidden: ssidHidden,
        custom_message: customMessage,
        led_valid: parseInt(ledValid, 10),
        active_display_type: activeDisplayType,
        enable_tamper_detect: enableTamperDetect,
        max_bits: maxBits,
        wiegand_wait_time: wiegandWait,
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
                alert('OpenDoorSim');
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
            updateCTFIndicator(data);
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
                const flag = card.flag || '';

                fetch(`/addUser?facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${name}&flag=${encodeURIComponent(flag)}`)
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

function editUser(index) {
    fetch('/getUsers')
        .then(res => res.json())
        .then(data => {
            const user = data[index];
            if (!user) return;
            document.getElementById('newFacilityCode').value = user.facilityCode;
            document.getElementById('newCardNumber').value = user.cardNumber;
            document.getElementById('newName').value = user.name;
            document.getElementById('newFlag').value = user.flag || '';
            const saveBtn = document.getElementById('saveUserButton');
            const cancelBtn = document.getElementById('cancelEditButton');
            if (saveBtn) {
                saveBtn.textContent = 'Update';
                saveBtn.onclick = function() { saveEditedUser(index); };
            }
            if (cancelBtn) cancelBtn.style.display = 'inline-block';
        })
        .catch(err => console.error('Error fetching users for edit:', err));
}

function saveEditedUser(index) {
    const facilityCode = document.getElementById('newFacilityCode').value;
    const cardNumber = document.getElementById('newCardNumber').value;
    const name = document.getElementById('newName').value;
    const flag = document.getElementById('newFlag').value;
    fetch(`/updateUser?index=${index}&facilityCode=${facilityCode}&cardNumber=${cardNumber}&name=${encodeURIComponent(name)}&flag=${encodeURIComponent(flag)}`)
        .then(response => {
            if (response.ok) {
                updateUserTable();
                alert('User updated successfully');
                // reset buttons
                const saveBtn = document.getElementById('saveUserButton');
                const cancelBtn = document.getElementById('cancelEditButton');
                if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
                if (cancelBtn) cancelBtn.style.display = 'none';
            } else {
                alert('Failed to update user');
            }
        })
        .catch(error => console.error('Error updating card:', error));
}

function cancelEdit() {
    const saveBtn = document.getElementById('saveUserButton');
    const cancelBtn = document.getElementById('cancelEditButton');
    if (saveBtn) { saveBtn.textContent = 'Save'; saveBtn.onclick = addUser; }
    if (cancelBtn) cancelBtn.style.display = 'none';
    // clear inputs
    document.getElementById('newFacilityCode').value = '';
    document.getElementById('newCardNumber').value = '';
    document.getElementById('newName').value = '';
    document.getElementById('newFlag').value = '';
}