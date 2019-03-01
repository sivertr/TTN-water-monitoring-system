function Decoder(bytes, port) {
 
  var temp = ((bytes[0] << 8) + bytes[1])/100.0; // reconstructs the temperature value from the first two bytes sent from the arduino
  var cond = ((bytes[2] << 8) + bytes[3])/100.0; // reconstruct conductivity
  var turb = ((bytes[4] << 8) + bytes[5])/100.0; // reconstruct tubidity
  var ox = ((bytes[6] << 8)+ bytes[7])/100.0;    // reconstruct dissolved oxygen
  
  return{
    data: [
      {
        type:"TEMPERATURE",
        value: temp
      },
      {
        type: "TURBIDITY",
        value: turb
      },
      {
        type: "CONDUCTIVITY",
        value: cond
      },
      {
        type: "DISSOLVED_OXYGEN",
        value: ox
      }
    ]
  };
}
