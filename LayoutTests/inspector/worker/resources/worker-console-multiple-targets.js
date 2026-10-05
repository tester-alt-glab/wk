onmessage = (event) => {
    switch (event.data.action) {
    case "log":
        console.log(event.data.text);
        break;
    case "error":
        console.error(event.data.text);
        break;
    case "clear":
        console.clear();
        break;
    }
};
